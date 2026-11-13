#include "wizard.h"

WizardComp const kComps [COMP_COUNT] = {
  {"Core runtime",          -1, 0,   NULL,      NULL,               NULL  },
  {"Runtime libraries",     0,  48,  "bin",     "fluxent_runtime",  "dll" },
  {"Fonts and icons",       0,  12,  "assets",  "segoe_fluent_map", "bin" },
  {"Developer tools",       -1, 0,   NULL,      NULL,               NULL  },
  {"Command-line tools",    3,  9,   "tools",   "xent_cli_command", "exe" },
  {"Debug symbols",         3,  140, "symbols", "fluxent_debug",    "pdb" },
  {"Samples",               -1, 0,   NULL,      NULL,               NULL  },
  {"Control gallery",       6,  22,  "samples", "gallery_page",     "c"   },
  {"Installer wizard",      6,  3,   "samples", "installer_wizard", "c"   },
  {"Offline documentation", -1, 31,  "docs",    "reference_topic",  "html"},
};

char const *const kFolders [FOLDER_COUNT] = {
  "C:\\Program Files\\Xent Sample",
  "C:\\Program Files (x86)\\Xent Sample",
  "D:\\Applications\\Xent Sample",
  "C:\\Users\\Public\\Xent Sample",
};

char const *const kChannels [CHANNEL_COUNT] = {"Stable", "Preview", "Nightly"};

char const *const kLicense
  = "XENT SAMPLE SOFTWARE LICENSE\n"
    "\n"
    "Please read these terms carefully before installing the software. By selecting \"I accept the terms\" "
    "and continuing, you agree to be bound by them. If you do not agree, cancel the installation.\n"
    "\n"
    "1. Grant. You may install and use any number of copies of the software on your devices to design, "
    "develop and test your programs.\n"
    "\n"
    "2. Scope. The software is licensed, not sold. These terms give you only some rights to use it. The "
    "authors reserve all other rights. You may not work around any technical limitations in the software, "
    "or reverse engineer it except where applicable law expressly permits.\n"
    "\n"
    "3. Sample code. The software contains sample code. You may modify, copy and distribute the sample code "
    "in source and object form, provided that you keep this notice in the copies you distribute.\n"
    "\n"
    "4. Updates. The software may check for updates on the schedule you choose during installation. You can "
    "change that schedule at any time. Updates replace earlier versions and are covered by these terms "
    "unless other terms accompany them.\n"
    "\n"
    "5. Data. The software does not collect personal data. The update check transmits the version number "
    "and the selected channel only.\n"
    "\n"
    "6. Export restrictions. You must comply with all domestic and international export laws and "
    "regulations that apply to the software.\n"
    "\n"
    "7. Support. Because the software is provided \"as is\", the authors may not provide support services "
    "for it.\n"
    "\n"
    "8. Disclaimer of warranty. THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR "
    "PURPOSE AND NONINFRINGEMENT.\n"
    "\n"
    "9. Limitation of liability. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, "
    "DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF "
    "OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
    "\n"
    "10. Entire agreement. These terms, and the terms for supplements and updates that you use, are the "
    "entire agreement for the software.";
