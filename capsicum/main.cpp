#include <Windows.h>
#include <stdio.h>

#define INJECTED_KEY_ID 0xDAC50F71

HHOOK g_keyboard_hook;

enum class Direction {
    UP, DOWN
};

enum class KeyState {
    IDLE = 0,
    HELD_DOWN_ALONE,
    HELD_DOWN_WITH_OTHER
};

void send_input(int scan_code, int virt_code, Direction direction) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.time = 0;
    input.ki.dwExtraInfo = (ULONG_PTR) INJECTED_KEY_ID;

    input.ki.wScan = scan_code;
    input.ki.wVk = virt_code;
    input.ki.dwFlags = direction == Direction::UP ? KEYEVENTF_KEYUP : 0;

    SendInput(1, &input, sizeof(INPUT));
}

KeyState g_key_state = KeyState::IDLE;

LRESULT CALLBACK keyboard_callback(int msg_code, WPARAM w_param, LPARAM l_param) {
    int swallow_input = 0;
    
    if (msg_code == HC_ACTION) {
        KBDLLHOOKSTRUCT* data = (KBDLLHOOKSTRUCT*) l_param;

        const Direction direction = (w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN)
            ? Direction::DOWN
            : Direction::UP;

        const bool is_injected = data->dwExtraInfo == INJECTED_KEY_ID;

        // CTRL
        const int ctrl_scan_code = 29;
        const int ctrl_vkey_code = 162;

        // ESCAPE
        const int esc_scan_code = 1;
        const int esc_vkey_code = 27;

        if (!is_injected) {
            if (data->scanCode == 58 && data->vkCode == 20) {
                swallow_input = 1;
                if (direction == Direction::DOWN) {
                    if (g_key_state == KeyState::IDLE) {
                        g_key_state = KeyState::HELD_DOWN_ALONE;
                    }
                }
                else { // UP
                    if (g_key_state == KeyState::HELD_DOWN_ALONE) {
                        send_input(esc_scan_code, esc_vkey_code, Direction::DOWN);
                        send_input(esc_scan_code, esc_vkey_code, Direction::UP);
                    }
                    else if (g_key_state == KeyState::HELD_DOWN_WITH_OTHER) {
                        send_input(ctrl_scan_code, ctrl_vkey_code, Direction::UP);
                    }

                    g_key_state = KeyState::IDLE;
                }
            }
            else {
                // Check if CAPS_LOCK is being held down
                if (direction == Direction::DOWN) {
                    if (g_key_state == KeyState::HELD_DOWN_ALONE) {
                        g_key_state = KeyState::HELD_DOWN_WITH_OTHER;
                        send_input(ctrl_scan_code, ctrl_vkey_code, Direction::DOWN);
                    }
                }
            }
        }
    }

    return (swallow_input) ? 1 : CallNextHookEx(g_keyboard_hook, msg_code, w_param, l_param);
}

int main() {
    printf("Capsicum Key Remapper for Windows\n");

    HWND window = GetConsoleWindow();
    HANDLE mutex = CreateMutex(NULL, TRUE, L"capsicum-key-remap.single-instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("capsicum.exe is already running!\n");
        goto end;
    }

    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_callback, NULL, 0);

    // Hide the Window
    ShowWindow(window, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

end:
    return 1;
}