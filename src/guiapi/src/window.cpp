//window.cpp

#include "window.hpp"
#include <algorithm> // std::find
#include "ScreenHandler.hpp"
#include "gui_timer.h"
#include "display.h"
#include "sound.hpp"
#include "marlin_client.h"

bool window_t::IsVisible() const { return flags.visible && !flags.hidden_behind_dialog; }
bool window_t::HasVisibleFlag() const { return flags.visible; };
bool window_t::IsHiddenBehindDialog() const { return flags.hidden_behind_dialog; }
bool window_t::IsEnabled() const { return flags.enabled; }
bool window_t::IsInvalid() const { return flags.invalid; }
bool window_t::IsFocused() const { return GetFocusedWindow() == this; }
bool window_t::HasTimer() const { return flags.timer; }
win_type_t window_t::GetType() const { return win_type_t(flags.type); }
bool window_t::IsDialog() const { return GetType() == win_type_t::dialog || GetType() == win_type_t::strong_dialog; }
bool window_t::ClosedOnTimeout() const { return flags.timeout_close == is_closed_on_timeout_t::yes; }
bool window_t::ClosedOnSerialPrint() const { return flags.serial_close == is_closed_on_serial_t::yes; }
bool window_t::HasEnforcedCapture() const { return flags.enforce_capture_when_not_visible; }
bool window_t::IsCapturable() const { return IsVisible() || HasEnforcedCapture(); }

void window_t::Validate(Rect16 validation_rect) {
    if (validation_rect.IsEmpty() || rect.HasIntersection(validation_rect)) {
        flags.invalid = false;
        validate(validation_rect);
    }
}

void window_t::Invalidate(Rect16 invalidation_rect) {
    if (invalidation_rect.IsEmpty() || rect.HasIntersection(invalidation_rect)) {
        invalidate(invalidation_rect);
        gui_invalidate();
    }
}

//frame will invalidate children
void window_t::invalidate(Rect16 validation_rect) {
    flags.invalid = true;
}

//frame will validate children
void window_t::validate(Rect16 validation_rect) {
}

// "IsCapturable() ? this : nullptr" does not work because of popup,
// popup does not claim capture, but can hide window
// At this point we are sure no dialog has capture, so we check only visible flag
window_t *window_t::GetCapturedWindow() {
    return HasVisibleFlag() ? this : nullptr;
}

void window_t::SetHasTimer() { flags.timer = true; }
void window_t::ClrHasTimer() { flags.timer = false; }
void window_t::Enable() { flags.enabled = true; }
void window_t::Disable() { flags.enabled = false; }
void window_t::SetEnforceCapture() { flags.enforce_capture_when_not_visible = true; }
void window_t::ClrEnforceCapture() { flags.enforce_capture_when_not_visible = false; }

void window_t::SetFocus() {
    if (!IsVisible() || !flags.enabled)
        return;
    if (focused_ptr == this)
        return;

    if (focused_ptr) {
        focused_ptr->Invalidate();
        focused_ptr->WindowEvent(focused_ptr, GUI_event_t::FOCUS0, 0); //will not resend event to anyone
    }
    focused_ptr = this;
    Invalidate();
    WindowEvent(this, GUI_event_t::FOCUS1, 0); //will not resend event to anyone
    gui_invalidate();
}

void window_t::Show() {
    if (!flags.visible) {
        flags.visible = true;
        //cannot invalidate when is hidden by dialog - could flicker
        if (!flags.hidden_behind_dialog)
            Invalidate();

        notifyVisibilityChange();
    }
}

void window_t::Hide() {
    if (flags.visible) {
        flags.visible = false;
        //cannot invalidate when is hidden by dialog - could flicker
        if (!flags.hidden_behind_dialog)
            Invalidate();

        notifyVisibilityChange();
    }
}

void window_t::notifyVisibilityChange() {
    if (GetParent())
        GetParent()->ChildVisibilityChanged(*this);
}

//do nothing screen/frame will do something ...
void window_t::ChildVisibilityChanged(window_t &child) {
}

void window_t::ShowAfterDialog() {
    if (flags.hidden_behind_dialog) {
        flags.hidden_behind_dialog = false;
        //must invalidate even when is not visible
        Invalidate();
    }
}

void window_t::HideBehindDialog() {
    if (!flags.hidden_behind_dialog) {
        flags.hidden_behind_dialog = true;
        //must invalidate - only part of window can be behind dialog
        Invalidate();

        //Validate would work with 1 dialog
        //cannot risk it
    }
}

bool window_t::IsShadowed() const {
    return flags.shadow == true;
}

void window_t::Shadow() {
    if (!flags.shadow) {
        flags.shadow = true;
        Invalidate();
    }
}
void window_t::Unshadow() {
    if (flags.shadow) {
        flags.shadow = false;
        Invalidate();
    }
}

color_t window_t::GetBackColor() const {
    if (flags.color_scheme_background && pBackColorScheme) {
        return pBackColorScheme->Get(IsFocused(), IsShadowed());
    }
    return color_back;
}

void window_t::SetBackColor(color_t clr) {
    color_back = clr;
    flags.color_scheme_background = false;
    Invalidate();
}

void window_t::SetBackColor(const color_scheme &clr) {
    pBackColorScheme = &clr;
    flags.color_scheme_background = true;
    Invalidate();
}

window_t::window_t(window_t *parent, Rect16 rect, win_type_t type, is_closed_on_click_t close)
    : parent(parent)
    , next(nullptr)
    , rect(rect)
    , flags(0)
    , color_back(GuiDefaults::ColorBack) {
    flags.type = uint8_t(type);
    flags.close_on_click = close;
    close == is_closed_on_click_t::yes ? Enable() : Disable();
    flags.visible = true; // do not call show, it needs parent to be registered
    Invalidate();
    if (parent)
        parent->RegisterSubWin(*this);
}

window_t::~window_t() {
    gui_timers_delete_by_window(this);
    if (GetFocusedWindow() == this)
        focused_ptr = nullptr;

    // if this window has captured, than it will be passed automaticaly to previous one
    // because last window in screen has it, no code needed

    //win_type_t::normal must be unregistered so ~window_frame_t can has functional linked list
    if (GetParent())
        GetParent()->UnregisterSubWin(*this);

    Screens::Access()->ResetTimeout();
}

Rect16 window_t::GetRect() const {
    if (GetParent()) {
        return GetParent()->TransformRect(rect); // do not use GetRect() - would be recursive
    }

    return rect;
}

Rect16 window_t::GetRectWithoutTransformation() const {
    return rect;
}

void window_t::SetRect(Rect16 rc) {
    if (GetParent()) {
        rect = GetParent()->TransformRect(rc); // do not use SetRect() - would be recursive
        return;
    }

    rect = rc;
}

void window_t::SetRectWithoutTransformation(Rect16 rc) {
    rect = rc;
}

//TransformRect calls GetRect which calls TransformRect on parrent level ...
Rect16 window_t::TransformRect(Rect16 rc) const {
    Rect16 this_rect = GetRect();
    if (flags.has_relative_subwins) {
        rc.Transform(this_rect);
    } else {
        rc = rc.Intersection(this_rect);
    }
    return rc;
}

void window_t::Reposition(Rect16::Top_t top) {
    SetRectWithoutTransformation(GetRectWithoutTransformation() = top);
}

void window_t::Reposition(Rect16::Left_t left) {
    SetRectWithoutTransformation(GetRectWithoutTransformation() = left);
}

void window_t::Resize(Rect16::Height_t height) {
    SetRectWithoutTransformation(GetRectWithoutTransformation() = height);
}

void window_t::Resize(Rect16::Width_t width) {
    SetRectWithoutTransformation(GetRectWithoutTransformation() = width);
}

void window_t::SetNext(window_t *nxt) {
    next = nxt;
}

/*
void window_t::SetPrev(window_t *prv) {
    prev = prv;
}
*/

void window_t::SetParent(window_t *par) {
    parent = par;
}

window_t *window_t::GetNext() const {
    return next;
}

/*
window_t *window_t::GetPrev() const {
    return prev;
}
*/

window_t *window_t::GetNextEnabled() const {
    if (next)
        return (next->IsEnabled()) ? next : next->GetNextEnabled();
    return nullptr;
}

/*
window_t *window_t::GetPrevEnabled() const {
    if (prev){
        return (prev->IsEnabled()) ? prev : prev->GetPrevEnabled();
    } else {
        return nullptr;
    }
}
*/

window_t *window_t::GetParent() const {
    return parent;
}

bool window_t::IsChildOf(window_t *win) const {
    window_t *par = GetParent();
    while (par) {
        if (par == win)
            return true;

        par = par->GetParent();
    }
    return false;
}

void window_t::Draw() {
    draw();
}

void window_t::draw() {
    if (IsInvalid() && rect.Width() && rect.Height()) {
        if (IsVisible()) {
            unconditionalDraw();
        }
        // there was in else branch: display::FillRect(rect, color_back);
        // I really do not know why, but it cannot be, because background was drawing over dialogs
        Validate();
    }
}

//window does not support subwindow elements, but window_frame does
bool window_t::RegisterSubWin(window_t &win) {
    //window must fit inside frame
    if (!GetRect().Contain(win.GetRect())) //could speed this up, but prefer smaller codesize
        return false;
    //parrent has relative subwins, child must have them too
    if (flags.has_relative_subwins)
        win.SetRelativeSubwins();

    Screens::Access()->ResetTimeout();

    return registerSubWin(win);
}

void window_t::UnregisterSubWin(window_t &win) {
    if (win.GetParent() != this)
        return;
    addInvalidationRect(win.GetRect());
    unregisterSubWin(win);
    Screens::Access()->ResetTimeout();
}

bool window_t::registerSubWin(window_t &win) {
    return false;
}

void window_t::unregisterSubWin(window_t &win) {
}

//cannot add rect, it is stored in frame, so must incalidate entire window
void window_t::addInvalidationRect(Rect16 rc) {
    if (!rect.IsEmpty()) {
        Invalidate();
    }
}

void window_t::unconditionalDraw() {
    display::FillRect(GetRect(), GetBackColor());
}

void window_t::WindowEvent(window_t *sender, GUI_event_t event, void *param) {
    static const char txt[] = "WindowEvent via public";
    windowEvent(EventLock(txt, sender, event), sender, event, param);
}

void window_t::ScreenEvent(window_t *sender, GUI_event_t event, void *param) {
    static const char txt[] = "ScreenEvent via public";
    EventLock(txt, sender, event); //just print debug msg
    screenEvent(sender, event, param);
}

//frame does something else - resend to all children
// MUST BE PRIVATE
// call nonvirtual ScreenEvent instead (contains debug output)
void window_t::screenEvent(window_t *sender, GUI_event_t event, void *param) {
    WindowEvent(sender, event, param);
}

// MUST BE PRIVATE
// call nonvirtual WindowEvent instead (contains debug output)
void window_t::windowEvent(EventLock /*has private ctor*/, window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::CLICK && parent) {
        if (flags.close_on_click == is_closed_on_click_t::yes) {
            Screens::Access()->Close();
        } else {
            parent->WindowEvent(this, event, param);
        }
    }
}

void window_t::ShiftNextTo(ShiftDir_t direction) {
    Shift(direction, rect.CalculateShift(direction));
}

void window_t::Shift(ShiftDir_t direction, uint16_t distance) {
    rect = Rect16(rect, direction, distance);
    Invalidate();
}

/*****************************************************************************/
//static

window_t *window_t::focused_ptr = nullptr;

window_t *window_t::GetFocusedWindow() {
    return focused_ptr;
}

void window_t::ResetFocusedWindow() {
    focused_ptr = nullptr;
}

/*****************************************************************************/
//capture
bool window_t::IsCaptured() const { return Screens::Access()->Get()->GetCapturedWindow() == this; }

bool window_t::EventEncoder(int diff) {
    marlin_notify_server_about_encoder_move();
    window_t *capture_ptr = Screens::Access()->Get()->GetCapturedWindow();
    if (diff == 0)
        return false;

    Screens::Access()->ScreenEvent(nullptr, GUI_event_t::ENC_CHANGE, (void *)(intptr_t)diff);

    if (!capture_ptr)
        return false;

    if (diff > 0) {
        capture_ptr->WindowEvent(capture_ptr, GUI_event_t::ENC_UP, (void *)(intptr_t)diff);
    } else {
        capture_ptr->WindowEvent(capture_ptr, GUI_event_t::ENC_DN, (void *)(intptr_t)-diff);
    }

    Screens::Access()->ResetTimeout();
    return true;
}

bool window_t::EventJogwheel(BtnState_t state) {
    static bool dont_click_on_next_release = false;
    marlin_notify_server_about_knob_click();
    window_t *capture_ptr = Screens::Access()->Get()->GetCapturedWindow();

    switch (state) {
    case BtnState_t::Pressed:
        Screens::Access()->ScreenEvent(nullptr, GUI_event_t::BTN_DN, 0);
        break;
    case BtnState_t::Released:
        Sound_Play(eSOUND_TYPE::ButtonEcho);
        Screens::Access()->ScreenEvent(nullptr, GUI_event_t::BTN_UP, 0);
        if (!dont_click_on_next_release && capture_ptr)
            capture_ptr->WindowEvent(capture_ptr, GUI_event_t::CLICK, 0);
        dont_click_on_next_release = false;
        break;
    case BtnState_t::Held:
        dont_click_on_next_release = true;
        if (capture_ptr)
            capture_ptr->WindowEvent(capture_ptr, GUI_event_t::HOLD, 0);
        break;
    }

    Screens::Access()->ResetTimeout();
    return true;
}

/*****************************************************************************/
//window_aligned_t

window_aligned_t::window_aligned_t(window_t *parent, Rect16 rect, win_type_t type, is_closed_on_click_t close)
    : AddSuperWindow<window_t>(parent, rect, type, close) {
    SetAlignment(GuiDefaults::Align());
}

Align_t window_aligned_t::GetAlignment() const {
    return (Align_t &)(flags.mem_array_u08[0]); //retype to Align_t reference, to avoid using private ctor
}

void window_aligned_t::SetAlignment(Align_t alignment) {
    flags.mem_array_u08[0] = (uint8_t &)(alignment);
    Invalidate();
}
