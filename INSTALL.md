# Installation guide

How to flash this RPN calculator firmware onto a Cardputer ADV without
setting up a development environment.

## Option 1: flash from your browser (recommended)

1. Open **[the install page](https://azek-dev.github.io/cardputer-rpn-calculator/)**
   in **Chrome** or **Edge** (it uses the Web Serial API, so Safari and
   Firefox won't work)
2. Connect the Cardputer ADV to your computer with a USB Type-C cable
3. Press the button on the page, pick your device from the port list, then
   press `INSTALL`
4. When flashing finishes the board reboots into the calculator

Updating later keeps your saved stack, memory registers and Wi-Fi settings.

This board deep-sleeps aggressively, so **press the G0 (BtnA) button on the
top edge to wake it before flashing**. That's the first thing to check if
the device doesn't show up in the port list.

## Option 2: flash manually with esptool.py

For environments where Web Serial isn't available. Requires Python.

1. Download these four files into the same folder:
   - [bootloader.bin](docs/firmware/bootloader.bin)
   - [partitions.bin](docs/firmware/partitions.bin)
   - [boot_app0.bin](docs/firmware/boot_app0.bin)
   - [firmware.bin](docs/firmware/firmware.bin)
2. Install esptool:
   ```bash
   pip install esptool
   ```
3. Connect the Cardputer ADV over USB Type-C and find its serial port
   - macOS: `ls /dev/cu.usbmodem*`
   - Windows: look for `COM*` in Device Manager
4. Run (replacing `<PORT>` with your port):
   ```bash
   python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
     --before default_reset --after hard_reset write_flash -z \
     --flash_mode dio --flash_freq 80m --flash_size 8MB \
     0x0000 bootloader.bin \
     0x8000 partitions.bin \
     0xe000 boot_app0.bin \
     0x10000 firmware.bin
   ```

## After flashing

Type a number and press Enter to push it onto the stack, then transform it
with operators and function names (`3` Enter `4` Enter `+` gives 7). See
[MANUAL.md](MANUAL.md) for the full walkthrough.

## Building from source

See the build section of [README.md](README.md); it needs
[PlatformIO Core](https://platformio.org/install/cli).
