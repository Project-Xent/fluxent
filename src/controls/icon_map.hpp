#pragma once

#include <cwchar>
#include <string>

namespace fluxent::controls {

// Mapping from common icon names (Segoe Fluent Icons / MDL2) to Unicode
// codepoints. We use wide characters for D2D compatibility.
struct IconDef {
  const wchar_t *codepoint;
};

inline const wchar_t *get_icon_codepoint(const std::string &name) {
  // Basic subset of Segoe Fluent Icons / MDL2
  if (name == "Accept" || name == "Check")
    return L"\uE8FB";
  if (name == "Add" || name == "Plus")
    return L"\uE710";
  if (name == "Minus")
    return L"\uE738";
  if (name == "Cancel" || name == "Close")
    return L"\uE711";
  if (name == "Settings" || name == "Gear")
    return L"\uE713";
  if (name == "Play")
    return L"\uE768";
  if (name == "Pause")
    return L"\uE769";
  if (name == "Stop")
    return L"\uE71A";
  if (name == "Previous")
    return L"\uE892";
  if (name == "Next")
    return L"\uE893";
  if (name == "GlobalNavButton")
    return L"\uE700";
  if (name == "Home")
    return L"\uE80F";
  if (name == "Edit")
    return L"\uE70F";
  if (name == "Delete" || name == "Trash")
    return L"\uE74D";
  if (name == "Save")
    return L"\uE74E";
  if (name == "Refresh")
    return L"\uE72C";
  if (name == "Search")
    return L"\uE721";
  if (name == "Back")
    return L"\uE72B";
  if (name == "Forward")
    return L"\uE72A";
  if (name == "Send")
    return L"\uE724";
  if (name == "Share")
    return L"\uE72D";
  if (name == "Download")
    return L"\uE896";
  if (name == "Upload")
    return L"\uE898";
  if (name == "More")
    return L"\uE712";
  if (name == "ChevronDown")
    return L"\uE70D";
  if (name == "ChevronUp")
    return L"\uE70E";
  if (name == "ChevronLeft")
    return L"\uE76B";
  if (name == "ChevronRight")
    return L"\uE76C";
  if (name == "Mic")
    return L"\uE720";
  if (name == "Volume")
    return L"\uE767";
  if (name == "Mute")
    return L"\uE74F";
  if (name == "Camera")
    return L"\uE722";
  if (name == "Video")
    return L"\uE714";
  if (name == "Mail")
    return L"\uE715";
  if (name == "Heart" || name == "Like")
    return L"\uEB51"; // Or E00B (solid)
  if (name == "Sound")
    return L"\uE767";

  return L""; // Not found
}

} // namespace fluxent::controls
