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

## Keeping both calculators on the device and using `switch`

This board has 8MB of flash and its partition table already carries two app
slots (`app0` at 0x10000 and `app1` at 0x340000, 3.19MB each). Each firmware
is only about 1.06MB, so the RPN and the algebraic calculator can both live
on the device at once: type `switch` and Enter in either one and it reboots
into the other, in about a second. No reflashing to go back and forth.

1. Get both firmware files, under distinct names:
   - this RPN calculator: [firmware.bin](docs/firmware/firmware.bin), saved as
     `rpn.bin` or `adv.bin` as appropriate
   - the algebraic calculator: [firmware.bin from cardputer-adv-calculator](https://github.com/azek-dev/cardputer-adv-calculator/raw/main/docs/firmware/firmware.bin)
2. Follow Method 2 above, but with this as the last command (`<PORT>` is your
   own serial port):
   ```bash
   python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
     --before default_reset --after hard_reset write_flash -z \
     --flash_mode dio --flash_freq 80m --flash_size 8MB \
     0x0000 bootloader.bin \
     0x8000 partitions.bin \
     0xe000 boot_app0.bin \
     0x10000 rpn.bin \
     0x340000 adv.bin
   ```
   `boot_app0.bin` at `0xe000` says "start app0 first", so what comes up right
   after flashing is whatever went to `0x10000` (the RPN calculator in this
   example). Either calculator can go in either slot. From then on `switch`
   moves between them.

To replace just one of them, flash only that slot's offset (`0x10000 rpn.bin`
alone, or `0x340000 adv.bin` alone). **PlatformIO's `pio run -t upload` always
writes to app0**, so updating whichever calculator lives in app1 needs an
explicit esptool offset as above.

Saved data doesn't collide: the stack and the history go into separate NVS
namespaces (`rpn` and `calc`) and survive independently, while the Wi-Fi
credentials and the sleep timeout are shared between the two.

## After flashing

Type a number and press Enter to push it onto the stack, then transform it
with operators and function names (`3` Enter `4` Enter `+` gives 7). See
[MANUAL.md](MANUAL.md) for the full walkthrough.

## Building from source

See the build section of [README.md](README.md); it needs
[PlatformIO Core](https://platformio.org/install/cli).
