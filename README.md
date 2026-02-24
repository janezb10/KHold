# KHold - Press-and-Hold Character Picker for Fcitx 5

KHold is a global module for Fcitx 5 that provides an intuitive character selection menu when a key is held down. It is designed for picking accented characters (like č, ć, š) or any other symbols without changing your keyboard layout.

## Features
- **Silent Typing:** Standard key behavior remains unchanged; symbols are committed on release for short presses.
- **Customizable:** Add any key-symbol pairs through the Fcitx 5 configuration GUI.
- **Horizontal Picker:** A clean, horizontal selection window that stays out of your way.
- **Visual Feedback:** The character is highlighted in the text only when the picker is active.
- **Adjustable Delay:** Fine-tune the hold duration to match your typing speed.
- **Bulk Import:** Quickly configure many keys at once using JSON or simple text formats.

## Prerequisites
You need the following packages installed to build KHold:

- **Debian/Ubuntu:** `libfcitx5core-dev`, `libfcitx5utils-dev`, `libfcitx5config-dev`, `nlohmann-json3-dev`, `cmake`, `extra-cmake-modules`, `build-essential`
- **Fedora:** `fcitx5-devel`, `nlohmann-json-devel`, `cmake`, `gcc-c++`
- **Arch Linux:** `fcitx5`, `nlohmann-json`, `cmake`, `base-devel`

## Build and Install

```bash
# 1. Clone the repository
git clone https://github.com/janezb10/KHold.git
cd KHold

# 2. Build the project
mkdir build && cd build
cmake ..
make

# 3. Install
sudo cp libkhold.so /usr/lib/x86_64-linux-gnu/fcitx5/libkhold.so
sudo cp khold.conf /usr/share/fcitx5/addon/khold.conf

# 4. Restart Fcitx 5
fcitx5 -r
```

## Configuration
1. Open **Fcitx 5 Configuration**.
2. Go to the **Addons** tab.
3. Find **KHold** and click the **Configure** (gear) icon.
4. **Delay:** Set your preferred long-press trigger time in milliseconds (default: 600ms).
5. **Entries:** Add your desired keys (e.g., `c`) and candidates (e.g., `č`, `ć`).
6. **Bulk Import (JSON string):** For quick setup, you can paste a JSON object to populate your entries. The values can be either a string of characters or an array of strings (allowing candidates with multiple characters).

   ```json
   {
     "keys": {
       "c": ["č", "ć", "ç"],
       "C": ["Č", "Ć", "Ç"],
       "s": ["š", "ß", "ś"],
       "S": ["Š", "Ś"],
       "z": ["ž", "ź", "ż"],
       "Z": ["Ž", "Ź", "Ż"],
       "d": ["đ"],
       "D": ["Đ"],
       "a": ["à", "á", "â", "ä", "æ", "ã", "å", "ā"],
       "A": ["À", "Á", "Â", "Ä", "Æ", "Ã", "Å", "Ā"],
       "e": ["è", "é", "ê", "ë", "ē", "ė", "ę"],
       "E": ["È", "É", "Ê", "Ë", "Ē", "Ė", "Ę"],
       "i": ["ì", "í", "î", "ï", "ī", "į", "ı"],
       "I": ["Ì", "Í", "Î", "Ï", "Ī", "Į"],
       "o": ["ò", "ó", "ô", "ö", "õ", "ø", "œ", "ō"],
       "O": ["Ò", "Ó", "Ô", "Ö", "Õ", "Ø", "Œ", "Ō"],
       "u": ["ù", "ú", "û", "ü", "ū"],
       "U": ["Ù", "Ú", "Û", "Ü", "Ū"],
       "n": ["ñ", "ń"],
       "N": ["Ñ", "Ń"],
       "l": ["ł"],
       "L": ["Ł"],
       "r": ["ř"],
       "R": ["Ř"],
       "y": ["ÿ"],
       "Y": ["Ÿ"],
       "$": ["€", "$", "£", "¥", "₩", "₽"],
       ".": ["…"],
       "?": ["¿", "?"],
       "!": ["¡", "!"],
       "-": ["–", "—", "•"]
     }
   }
   ```
   **Note:** Pasting JSON will add/update entries in the list above and clear the JSON input field.
7. Click **Apply**.

## Support the Project
If KHold makes your typing experience smoother, consider supporting its development:
- **GitHub Sponsors**
- **Buy me a coffee**

## Technical Details
- **Trigger Delay:** Configurable via GUI (100ms - 2000ms).
- **Global Module:** Implemented as an `AddonInstance` with a `PreInputMethod` event watcher.
- **Compatibility:** Optimized for Fcitx 5.1.12+.

## License
[LGPL-2.1-or-later](https://www.gnu.org/licenses/lgpl-2.1.html)
