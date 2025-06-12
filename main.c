#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"
#include "cyw43.h"

// Define a general buffer size for transfers.
// Buffer size of 1026 bytes should be enough because:
// - 1021 bytes: maximum ACL payload according to the cyw43's HCI_READ_BUFFER_SIZE_COMMAND response
// - 4 bytes: HCI ACL header (2 bytes for handle and flags, 2 bytes for data length)
// - 1 byte: H4 packet type indicator
// Total: 1021 + 4 + 1 = 1026 bytes
// Since we have no shortage of memory we stay way above that just to be safe.
#define BUFFER_SIZE 2048

// HCI send buffer for CDC->HCI
static uint8_t hci_tx_buf[BUFFER_SIZE];

// Hex debug print helper - disable by setting HCI_DEBUG_LOG to false
#if HCI_DEBUG_LOG
static void dump_hex(const char *prefix, const uint8_t *data, size_t len) {
    char line[128];  // Enough for prefix + 16 bytes (3 chars each) + newline
    size_t offset = 0;
    
    while (offset < len) {
        int pos = 0;
        pos += snprintf(line + pos, sizeof(line) - pos, "%s", prefix);
        for (int i = 0; i < 16 && offset + i < len; i++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[offset + i]);
        }
        line[pos++] = '\r';
        line[pos++] = '\n';
        tud_cdc_n_write(1, line, pos);
        tud_cdc_n_write_flush(1);
        offset += 16;
    }
}
#else
static void dump_hex(const char *prefix, const uint8_t *data, size_t len) {}
#endif

// HCI USB RX state for incremental packet reads
typedef struct {
    uint8_t buffer[BUFFER_SIZE];   // Full packet buffer
    uint32_t expected_len;         // Total length we expect (type + preamble + payload)
    uint32_t received_len;         // How much we've received so far
    bool in_progress;              // Whether we're currently assembling a packet
} hci_usb_rx_state_t;

static hci_usb_rx_state_t rx_state = {0};

// Process USB CDC -> HCI direction with packet framing awareness
static void process_usb_to_hci(void) {
    if (!tud_cdc_n_connected(0)) return;
    
    // If we're not already receiving a packet, start a new one
    if (!rx_state.in_progress) {
        if (tud_cdc_n_available(0) < 1) return;  // Need at least the packet type
        
        // Read packet type
        tud_cdc_n_read(0, &rx_state.buffer[0], 1);
        uint8_t packet_type = rx_state.buffer[0];
        
        uint32_t preamble_size = 0;
        
        // Determine preamble size based on packet type
        switch (packet_type) {
            case 0x01: preamble_size = 3; break;  // HCI Command
            case 0x02: preamble_size = 4; break;  // HCI ACL
            case 0x03: preamble_size = 3; break;  // HCI SCO
            default:
            dump_hex("Unsupported: ", &packet_type, 1);
            // Unknown or unsupported packet type — discard it
            return;
        }
        
        // Wait until the entire preamble is available
        if (tud_cdc_n_available(0) < preamble_size) {
            return;  // Not enough data yet, wait for more
        }
        
        // Read preamble bytes
        uint32_t cdc_len = tud_cdc_n_read(0, &rx_state.buffer[1], preamble_size);
        
        // Determine payload length based on packet type and preamble
        uint32_t payload_len = 0;
        if (packet_type == 0x01) {
            payload_len = rx_state.buffer[3];  // HCI Command param length
        } else if (packet_type == 0x02) {
            payload_len = rx_state.buffer[3] | (rx_state.buffer[4] << 8);  // HCI ACL data length
        } else if (packet_type == 0x03) {
            payload_len = rx_state.buffer[3];  // HCI SCO data length
        }
                
        // Use the MIN macro to prevent RX buffer overflow.
        // Ideally, we should reset the pico here, as there is no sensible way to recover from this condition.
        // That said, this scenario should never occur because the buffer size exceeds the maximum payload the controller can produce.
        rx_state.expected_len = MIN(1 + preamble_size + payload_len, BUFFER_SIZE);
        rx_state.received_len = 1 + preamble_size;
        rx_state.in_progress = true;
    }
    
    // Continue reading the remaining payload
    uint32_t remaining = rx_state.expected_len - rx_state.received_len;
    if (remaining > 0) {
        uint32_t available = tud_cdc_n_available(0);
        uint32_t to_read = MIN(remaining, available);
        
        if (to_read > 0) {
            tud_cdc_n_read(0, &rx_state.buffer[rx_state.received_len], to_read);
            rx_state.received_len += to_read;
        }
    }
    
    // Once we have the full packet, send it to the Bluetooth HCI
    if ((rx_state.received_len == rx_state.expected_len)) {
        dump_hex("> ", rx_state.buffer, rx_state.expected_len);
        uint8_t header[] = {0, 0, 0};
        memcpy(hci_tx_buf, header, 3);
        memcpy(hci_tx_buf+3, rx_state.buffer, rx_state.expected_len);
        int ret = cyw43_bluetooth_hci_write(hci_tx_buf, rx_state.expected_len+3);
        
        // Reset for the next packet
        rx_state.in_progress = false;
        rx_state.received_len = 0;
        rx_state.expected_len = 0;
    }
}

// Process HCI -> USB CDC direction
static void process_hci_to_usb(void) {
    uint8_t hci_buffer[BUFFER_SIZE];
    uint32_t len;
    // Read any available HCI data from the CYW43 driver.
    int res = cyw43_bluetooth_hci_read(hci_buffer, BUFFER_SIZE, &len);
    
    if (!res && len > 0) {
        dump_hex("< ", &hci_buffer[3], len-3);
        uint32_t written = tud_cdc_n_write(0, &hci_buffer[3], len-3);
        if (written > 0) {
            tud_cdc_n_write_flush(0);
        }
    }
}

// Second loop to receive data from HCI (Controller) and send to CDC (Host)
void hci_to_usb_task(void) {
    while (true) {
        tud_task();
        process_hci_to_usb();		
    }
}

int main(void) {
    
    // Initialize Cypress chip
    if (cyw43_arch_init()) {
        return -1;
    }
    cyw43_init(&cyw43_state);
    sleep_ms(500);
    
    // Initialize TinyUSB stack
    tusb_init();
    sleep_ms(500);
    
    multicore_launch_core1(hci_to_usb_task);
    
    // Main loop to receive data via CDC (Host) and send to HCI (Controller)
    while (true) {
        tud_task();
        process_usb_to_hci();
    }
    
    return 0;
}
