"""PlatformIO post-build step: one flashable image for the web installer.

Writes .pio/build/goli/goli-firmware.bin (bootloader, partitions, boot_app0 and the app
merged at their offsets), which ESP Web Tools flashes at 0x0.
"""
Import("env")  # noqa: F821  (provided by PlatformIO)


def merge_bin(source, target, env):
    out = env.subst("$BUILD_DIR/goli-firmware.bin")
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), str(target[0])))
    args = " ".join(f'{offset} "{path}"' for offset, path in sorted(images, key=lambda i: int(i[0], 16)))
    cmd = (f'"$PYTHONEXE" "$UPLOADER" --chip ${{BOARD_MCU}} merge-bin -o "{out}" '
           f'--flash-mode ${{__get_board_flash_mode(__env__)}} --flash-size 16MB {args}')
    env.Execute(env.VerboseAction(cmd, "Merging " + out))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
