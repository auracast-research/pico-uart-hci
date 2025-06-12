# Raspberry Pi Pico UART HCI

This project exposes the Raspberry Pi Pico W's Bluetooth Controller HCI
interface via USB CDC. This allows using the Pico W as UART HCI dongle, for
example, with `hciattach` on Linux.

## Why?

Because we wanted to use the Pico W's Bluetooth controller. You could also just
buy another dongle with an Infineon/Cypress/Broadcom chip (like [this
one](https://www.ezurio.com/part/bt851)). But why buy something if you can build
it with stuff you already have.

More on that in our [blogpost](todo).

## Building the Project

1. Install the [Pico SDK](https://github.com/raspberrypi/pico-sdk)
1. Build the project.
```bash
git clone https://github.com/ttdennis/pico-uart-hci.git
cd pico-uart-hci
mkdir build
cd build
cmake -DPICO_BOARD=pico_w ..
make
```

1. [Flash](https://projects.raspberrypi.org/en/projects/get-started-pico-w/1) the `uf2` file the Pico W.
1. Attach the Pico as Bluetooth controller (Linux).
```bash
# start bluetooth if it's not running
sudo systemctl start bluetooth
# attach the HCI UART
sudo hciattach /dev/ttyACM0 any
# list HCI devices
hciconfig
hci0:   Type: Primary  Bus: UART
        BD Address: 28:CD:C1:XX:XX:XX  ACL MTU: 1021:8  SCO MTU: 64:10
        DOWN RUNNING
        RX bytes:722 acl:0 sco:0 events:40 errors:0
        TX bytes:438 acl:0 sco:0 commands:40 errors:0
# power on device
sudo hciconfig hci0 up
```

Now you can use the Pico as Bluetooth controller. For example with
`bluetoothctl`
```bash
$ bluetoothctl
Agent registered
[bluetoothctl]> power on
Changing power on succeeded
S[bluetoothctl]> scan on
SetDiscoveryFilter success
Discovery started
[CHG] Controller 28:CD:C1:XX:XX:XX Discovering: yes
[NEW] Device XX:XX:XX:XX:XX:XX XX-XX-XX-XX-XX-XX

[...]
```

By default, debug logging via a second CDC interface is enabled. If you wish to
deactivate it, set `HCI_DEBUG_LOG` to `0` in the
[CMakeLists.txt](./CMakeLists.txt) file.

## Limitations

HCI and ACL work well, but SCO currently does not work. More details about the
issue can be found in the [pico-sdk
repo](https://github.com/raspberrypi/pico-sdk/issues/1461). Apparently, this is
a combination of an issue with the controller firmware, and the way the
controller is wired to the Pico SoC. By default, the controller routes audio via
I2S, which is not connected on the Pico, and routing it over HCI does not seem
to work at the moment.

