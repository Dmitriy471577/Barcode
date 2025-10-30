#include "BarcodeReader.h"
#include <windows.h>

using namespace std;

int main() {
    BarcodeReader reader;
    setlocale(LC_ALL, "Russian");
    wchar_t filePath[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tiff\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    while (true) {
        if (GetOpenFileNameW(&ofn)) {
            // ����������� ����
            wstring ws(filePath);
            string path(ws.begin(), ws.end());

            // �������������
            string result = reader.detectAndDecodeBarcode(path);

            // ����� ����������
            wstring wresult(result.begin(), result.end());
            MessageBoxW(nullptr, wresult.c_str(), L"��������� �������������", MB_OK);
        }
        else {
            MessageBoxW(nullptr, L"���� �� ������!", L"����������", MB_OK);
        }
    }
    return 0;
}