#pragma once

// Gameplay option renderers still use the ImGui spelling at their call sites.
// Keep the compatibility mapping isolated from module declarations so a module
// header no longer pulls the complete GUI and every other module into a TU.
#include "../gui/widgets.hpp"

#ifndef THEDORIUS_UI_NO_REDIRECT
#define Checkbox ThedoriusCheckbox
#define SliderFloat ThedoriusSliderFloat
#define SliderInt ThedoriusSliderInt
#define SliderFloat2 ThedoriusSliderFloat2
#define SliderInt2 ThedoriusSliderInt2
#define Combo ThedoriusCombo
#define ColorEdit4 ThedoriusColorEdit4
#define SeparatorText ThedoriusSeparatorText
#define TextDisabled ThedoriusStaticDescription
#define TextWrapped ThedoriusStaticDescription
#endif
