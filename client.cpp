#include<WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"WS2_32.lib")
#include "Receive.h"

#include<string>
#include<iostream>
#include<thread>
#include<mutex>
#include<chrono>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
std::recursive_mutex mtx;

Receive read;
struct ChatState {
    SOCKET UserSocket = INVALID_SOCKET;
    std::string chatHistory = "";
    std::string errorstat;
    std::vector<std::pair<std::string, int>>cacheVec;
    bool running = true;
    bool isAuthorized = false;
    bool waitres = false;
    bool userisfind = false;
    int currentRoom = 0;
};
void Learning(ChatState&state) {//Функция дял чтения,пока тут
 //   std::string welcome = read.receive(UserSocket);
    while (true) {       
 std::string getmessages = read.receive(state.UserSocket);//взять строку у ресива
 if (!getmessages.empty()) {
     std::lock_guard<std::recursive_mutex>hisLock(mtx);
      if (getmessages.find("USER_FIND|") != std::string::npos) {//USER_FIND|ник|айди
                std::string newuser = getmessages.substr(10);
                if (!newuser.empty()) {
                    std::string nickcashe;
                    std::string idfind;

                    for (auto it = newuser.begin(); it != newuser.end(); it++) {
                        char c = *it;
                        if (c != '|') {
                            nickcashe.push_back(c);
                        }
                        else {
                            break;
                        }
                    }
                    idfind = newuser.substr(nickcashe.size() + 1);
                    if (!idfind.empty()) {
                        int idd = std::stoi(idfind);
                        state.cacheVec.clear();//чтобы не всплыл предыдущий айди
                        state.cacheVec.push_back({ nickcashe, idd });
                        state.userisfind = true;
                    }
                }
            }
     if (getmessages.find("NEW_ROOM|") != std::string::npos) {
     std::string curroom = getmessages.substr(9);
     state.currentRoom = std::stoi(curroom);
     state.chatHistory.clear();
     std::string chooseroom = "CURRENT_ROOM|" + (curroom)+"\n";
     send(state.UserSocket, chooseroom.c_str(), (int)chooseroom.size(), 0);

 }
 
 else {
     {
         std::lock_guard<std::recursive_mutex>myLock(mtx);
         state.errorstat = getmessages;
     }
 }

          
            if (state.waitres == true) {
                if (getmessages.find("Auth_OK|") != std::string::npos || getmessages.find("Register_OK|") != std::string::npos) {
                    {
                        std::lock_guard < std::recursive_mutex>llock(mtx);
                        state.isAuthorized = true;
                        state.errorstat = "";
                    }
                }
               
                state.waitres = false;
                continue;//след итерация на ресив
            }
            state.chatHistory += getmessages ;
        
        }
        else {
            state.chatHistory += "SYSTEM:Connection Error\n";//тк если врнклось 0 байт то коннекта нет
            state.isAuthorized = false;
            state.waitres = false;
            break;
        }
    }
}
void addPersData(char(&nick)[32], char(&pass)[32]) {
    ImGui::InputText("Login", nick, sizeof(nick));
    ImGui::InputText("Password", pass, sizeof(pass),ImGuiInputTextFlags_Password);
   
}
enum class FirstStat {
    CHOOSE,
    SIGNIN,
    REGISTRATION
};

bool ConToServ(SOCKET& servsock) {
    if (servsock != INVALID_SOCKET) {//Если сокет открыт
        closesocket(servsock);
        servsock = INVALID_SOCKET;
    }
    servsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servsock == INVALID_SOCKET) {
        closesocket(servsock);
        return 1;
    }
    sockaddr_in clientADDR;
    clientADDR.sin_family = AF_INET;
    clientADDR.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &clientADDR.sin_addr) <= 0) {
        closesocket(servsock);//Если сокет умер до создания-удаляю
        servsock =INVALID_SOCKET;
        return false;//не подключился
    }
    if (connect(servsock, (sockaddr*)&clientADDR, sizeof(clientADDR)) < 0) {
        closesocket(servsock);//логика с удалением та же
        servsock = INVALID_SOCKET;
        return false;//не подключился
        //continue;//пока будет тут до логики реконнекта
    }
    return true;//подключение успешно
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {//Главная функция 
    bool IsConnect = false;

    WSADATA wsa;//Адрес
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {//Инициализация сразу с проверкой
        return 1;
    };

    char nickname[32] = "";
    char message[256] = "";
    char password[32] = "";
    char  findnick[32] = "";

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Chat Class", nullptr };
    ::RegisterClassExW(&wc);
    ChatState states;
    std::jthread learn;
    bool isConnected = ConToServ(states.UserSocket);
    if (isConnected == true) {
        learn = std::jthread([&states]() {
            Learning(states);
            });
    }

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Chat Client", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance); // ИСПРАВЛЕНО
        return 1;

    }

    ::ShowWindow(hwnd, nCmdShow);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);


    FirstStat statusWindow = FirstStat::CHOOSE;//По дефолту первое окно выбора
    while (states.running) {

        // 1. опрос событий ОС (мышь, клавиатура, закрытие окна)
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT || msg.message == WM_CLOSE || msg.message == WM_DESTROY) {//логика выхода
                states.running = false; // Выход из цикла при закрытии окна
            }
        }
        if (states.UserSocket == INVALID_SOCKET) {
            std::string qstr = "QUIT\n";
            send(states.UserSocket, qstr.c_str(), (int)qstr.size(), 0);
            closesocket(states.UserSocket);
            states.UserSocket = INVALID_SOCKET;
        }
        if (!states.running) {
            break;
        }
        // 2. ПОДГОТОВКА НОВОГО ГРАФИЧЕСКОГО КАДРА
        ImGui_ImplDX11_NewFrame();  // Сброс состояния рендера DirectX
        ImGui_ImplWin32_NewFrame(); // Сброс состояния окна Windows
        ImGui::NewFrame();          // Старт сборки нового кадра ImGui

        // 3. отрисовка интерфейса
        if (!states.isAuthorized) {

            switch (statusWindow) {
            case (FirstStat::CHOOSE):
                ImGui::Begin("Welcome!");
                if (ImGui::Button("Sign In")) {
                    statusWindow = FirstStat::SIGNIN;

                }
                else if (ImGui::Button("Create an account")) {
                    statusWindow = FirstStat::REGISTRATION;
                }
                ImGui::End();
                break;
            case(FirstStat::SIGNIN): {
                ImGui::Begin("Sign IN");
                addPersData(nickname, password);
                ImGui::Separator();
                if (states.waitres == true) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Checking data... Please wait.");
                }
                else {
                    if (ImGui::Button("Sign In")) {
                        if (strlen(nickname) > 0 && strlen(password) > 0) {
                            //  std::lock_guard<std::mutex>signLock(mtx);//защита для waitres и errorstat
                            states.waitres = true;//ожидание проверки данных сервером
                            states.errorstat = "";//очистка ошибок от предыдущего ввода
                            std::string authstr = "SIGNIN|" + std::string(nickname) + "|" + std::string(password) + "\n";
                            send(states.UserSocket, authstr.c_str(), (int)authstr.size(), 0);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Back", ImVec2(100, 30))) {
                        statusWindow = FirstStat::CHOOSE;
                        states.errorstat = "";
                    }
                }
                if (!states.errorstat.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), states.errorstat.c_str());
                }

                ImGui::End();
                break;
            }
            case(FirstStat::REGISTRATION): {
                ImGui::Begin("Registration");
                addPersData(nickname, password);
                ImGui::Separator();
                if (states.waitres == true) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Checking data... Please wait.");
                }
                else {
                    if (ImGui::Button("Create a new account")) {
                        //     std::lock_guard<std::mutex>refMtx(mtx);
                        states.waitres = true;
                        states.errorstat = "";
                        std::string regstr = "REGISTRATION|" + std::string(nickname) + "|" + std::string(password) + "\n";
                        send(states.UserSocket, regstr.c_str(), (int)regstr.size(), 0);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Back", ImVec2(100, 30))) {
                    statusWindow = FirstStat::CHOOSE;
                    states.errorstat = "";
                }
                if (!states.errorstat.empty()) {
                    ImGui::Text("This login is already in use, try using another one.");
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), states.errorstat.c_str());
                }
                ImGui::End();
                break;
            }
            }
        }
        else if (states.isAuthorized == true) {//переход на след.окно
            ImGui::Begin("LOBBY");

            ImGui::Separator();
            ImGui::BeginChild("finduser", ImVec2(0, 35));
            ImGui::PushItemWidth(150);
            ImGui::InputText("Find User", findnick, sizeof(findnick));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Find!", ImVec2(60, 0))) {
                if (strlen(findnick) > 0) {
                    std::string finduser = "FIND_USER|" + std::string(findnick) + "\n";
                    send(states.UserSocket, finduser.c_str(), (int)finduser.size(), 0);
                }
            }

            ImGui::EndChild();//закрыть панель поиска
            if (states.userisfind == true) {
                if(!states.cacheVec.empty()){
                std::string mbw = states.cacheVec.front().first;
                ImGui::PushItemWidth(70);
                ImGui::PopItemWidth();
                ImGui::Text("Found: %s", mbw.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Start chatting", ImVec2(100, 0))) {
                    int userid = states.cacheVec.front().second;//айди собеседника
                    std::string startls = "START_LS|" + std::to_string(userid) + "\n";//создается комната в бд
                    send(states.UserSocket, startls.c_str(), (int)startls.size(), 0);
                    states.userisfind = false;//послк поиска плашка уберется
                }
            }
        }
            if (states.currentRoom == 0) {

                ImGui::Text("Please select a chat...");

            }
            else {

                ImGui::Separator();
                ImGui::BeginChild("ScrollZone", ImVec2(0, -60), ImGuiChildFlags_Borders);

                {
                    std::lock_guard<std::recursive_mutex>hismtx(mtx);
                    ImGui::TextWrapped(states.chatHistory.c_str());//Перенос истории с автоперносами текста
                }
                ImGui::SetScrollHereY(1.0f);

                ImGui::EndChild();
                ImGui::Separator();//отделение от зоны ввода сообщений

                //Ввод сообщений
                bool pressEnter = ImGui::InputText("Write here", message, sizeof(message), ImGuiInputTextFlags_EnterReturnsTrue);//вернет true при нажатии
                ImGui::SameLine();//т.е. будет на строке со вводом
                bool pressed = ImGui::Button("Send");
                if (pressEnter || pressed) {
                    if (strlen(message) > 0) {
                        std::string msgstr = message;
                        if (msgstr.rfind("/nick", 0) == 0) {
                            std::string newNick = msgstr.substr(6);
                            if (!newNick.empty()) {
                                std::string nms = "CHANGE_NICK|" + newNick + "\n";
                                {
                                    std::unique_lock<std::recursive_mutex>nmtx(mtx);
                                    send(states.UserSocket, nms.c_str(), (int)nms.size(), 0);
                                }
                                strcpy_s(nickname, newNick.c_str());//смена ника в шапке
                            }
                        }
                        else {
                            std::string fullmsg = "MSG|" + std::to_string(states.currentRoom) + "|" + msgstr + "\n";
                            {
                                std::lock_guard<std::recursive_mutex>msgmtx(mtx);
                                send(states.UserSocket, fullmsg.c_str(), (int)fullmsg.size(), 0);
                                states.chatHistory += std::string(nickname) + "|" + msgstr + "\n";
                            }

                            msgstr.clear();
                            memset(message, 0, sizeof(message));//мемсетаем для очистки буффера
                        }
                    }
                }
            }
                if (ImGui::Button("log out")) {
                    std::string quitstr = "quit\n";
                    {

                        std::lock_guard<std::recursive_mutex>qmtxt(mtx);
                        send(states.UserSocket, quitstr.c_str(), (int)quitstr.size(), 0);
                    }
                    states.isAuthorized = false; // возврат к предыдущему состоянию
                }

                ImGui::End();
            }
        
               
            






            // 4. ОЧИСТКА СТАРЫХ ДАННЫХ С ЭКРАНА И ВЫВОД НОВОГО КАДРА (РЕНДЕРИНГ)
            ImGui::Render(); // Расчет геометрии интерфейса ImGui
            const float clear_color[4] = { 0.15f, 0.15f, 0.15f, 1.00f }; // сделал массивом из 4-х элементов RGBA цвет фона

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr); // Выбор буфера кадра
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color); // Очистка экрана цветом фона

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // Отрисовка геометрии ImGui силами DirectX
            g_pSwapChain->Present(1, 0); // Подача готовой картинки на монитор с V-Sync
        }
        states.running = false;
        if (states.UserSocket != INVALID_SOCKET) {
            closesocket(states.UserSocket);
            states.UserSocket = INVALID_SOCKET;
        }
        // 5. ДЕИНИЦИАЛИЗАЦИЯ И ОСВОБОЖДЕНИЕ ПАМЯТИ
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        ::ExitProcess(0);
        return 0;
    }


bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};//массив уровней возможностей
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

