#include "main.h"

#pragma comment(lib, "Ws2_32.lib")

DWORD				g_SAMP;
HANDLE				g_DllHandle;
bool				work = false;
bool				enable = true;
bool				spammed = false;

int					sockState = 1;

std::queue<std::string> chat;

uint8_t _declspec (naked) hook_handle_rpc_packet(void)
{
	static RPCParameters* pRPCParams = nullptr;
	static RPCNode* pRPCNode = nullptr;
	static DWORD dwTmp = 0;

	__asm pushad;
	__asm mov pRPCParams, eax;
	__asm mov pRPCNode, edi;

	HandleRPCPacketFunc(pRPCNode->uniqueIdentifier, pRPCParams, pRPCNode->staticFunctionPointer);
	dwTmp = g_SAMP + SAMP_HOOKEXIT_HANDLE_RPC;

	__asm popad;
	__asm add esp, 4 // overwritten code
	__asm jmp dwTmp;
}

uint8_t _declspec (naked) hook_handle_rpc_packet2(void)
{
	static RPCParameters* pRPCParams = nullptr;
	static RPCNode* pRPCNode = nullptr;
	static DWORD dwTmp = 0;

	__asm pushad;
	__asm mov pRPCParams, ecx;
	__asm mov pRPCNode, edi;

	HandleRPCPacketFunc(pRPCNode->uniqueIdentifier, pRPCParams, pRPCNode->staticFunctionPointer);
	dwTmp = g_SAMP + SAMP_HOOKEXIT_HANDLE_RPC2;

	__asm popad;
	__asm jmp dwTmp;
}


void nnMain()
{
	Sleep(30000);

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SockThread, NULL, 0, NULL);

	g_SAMP = (DWORD)GetModuleHandle("samp.dll");

	g_stSAMP = stGetSampInfo();

	SetupSAMPHook("HandleRPCPacket", SAMP_HOOKENTER_HANDLE_RPC, hook_handle_rpc_packet, DETOUR_TYPE_JMP, 6, "FF5701");
	SetupSAMPHook("HandleRPCPacket2", SAMP_HOOKENTER_HANDLE_RPC2, hook_handle_rpc_packet2, DETOUR_TYPE_JMP, 8, "FF5701");

	ULONG lUpdateTick = 0;

	while (true)
	{
		if (GetTickCount64() - lUpdateTick > 500)
		{
			g_stSAMP = stGetSampInfo();

			if (isBadPtr_writeAny(g_stSAMP, sizeof(stSAMP)))
				return;

			if (isBadPtr_writeAny(g_stSAMP->pPools, sizeof(stSAMPPools)))
				return;

			g_Chat = stGetSampChatInfo();
			if (isBadPtr_writeAny(g_Chat, sizeof(stChatInfo)))
				return;

			g_Input = stGetInputInfo();
			if (isBadPtr_writeAny(g_Input, sizeof(stInputInfo)))
				return;
		}

		if (GetAsyncKeyState(VK_F1))
		{
			enable = !enable;
			Sleep(500);
		}

		if (GetAsyncKeyState(VK_F10))
		{
			sockState++;
			if (sockState == 3)
				sockState = 0;
			
			addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMIT] Socket State to %d", sockState);
			Sleep(500);
		}

		if (!enable) work = false;
		if (work == true)
		{
			keybd_event(VK_TAB, 0, 0, 0);
			Sleep(250);
			keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
			keybd_event(VK_ESCAPE, 0, 0, 0);
			Sleep(250);
			keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);

			Sleep(2000);

			keybd_event(VK_TAB, 0, 0, 0);
			Sleep(250);
			keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
			keybd_event(VK_ESCAPE, 0, 0, 0);
			Sleep(250);
			keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);

			work = false;
		}
		Sleep(10);
	}
}

void HandleRPCPacketFunc(unsigned char id, RPCParameters* rpcParams, void(*callback) (RPCParameters*))
{
	if (rpcParams != nullptr && rpcParams->numberOfBitsOfData >= 8 && work == false && enable == true)
	{
		switch (id)
		{
			case RPC_DisplayGameText:
			{
				BitStream bsData(rpcParams->input, rpcParams->numberOfBitsOfData / 8, false);
				int style, time;
				bsData.Read(style);
				bsData.Read(time);

				int textLen;
				bsData.Read(textLen);

				char* text = new char[textLen];
				bsData.Read(text, textLen);

				if (strcmp(text, "~r~~n~|~r~~h~|~y~~h~|~y~|~g~~h~|~g~|") == 0)
				{
					work = true;
				}
				break;
			}
			case RPC_ClientMessage:
			{
				BitStream		bsData(rpcParams->input, rpcParams->numberOfBitsOfData / 8, false);
				uint32_t		dwStrLen, dwColor;
				char			szMsg[256];

				bsData.Read(dwColor);
				bsData.Read(dwStrLen);
				if (dwStrLen >= sizeof(szMsg)) dwStrLen = sizeof(szMsg) - 1;
				bsData.Read(szMsg, dwStrLen);
				szMsg[dwStrLen] = '\0';

				if(sockState == 0) chat.push(std::string(szMsg));
				else
				{
					std::queue<std::string> empty;
					chat.swap(empty);
				}
				
				break;
			}
			case RPC_Chat:
			{
				BitStream		bsData(rpcParams->input, rpcParams->numberOfBitsOfData / 8, false);
				static char		last_clientmsg[SAMP_MAX_PLAYERS][256];
				static DWORD	allow_show_again = 0;
				uint16_t		playerId = uint16_t(-1);
				uint8_t			byteTextLen;
				char			szText[256];

				bsData.Read(playerId);

				bsData.Read(byteTextLen);
				bsData.Read(szText, byteTextLen);
				szText[byteTextLen] = '\0';

				if (sockState == 0) chat.push(std::string(szText));
				else
				{
					std::queue<std::string> empty;
					chat.swap(empty);
				}

				break;
			}
		}
	}
	callback(rpcParams);
}

unsigned __stdcall ClientRecvThr(void* a) 
{

	Socket* s = (Socket*)a;

	while (true) 
	{
		if (sockState != 1)
		{
			s->Close();
			break;
		}

		std::string r = s->ReceiveLine();
		if (r.empty())
		{
			addMessageToChatWindow(D3DCOLOR_XRGB(0xFF, 0xFF, 0xFF), "[CHAT EMITTER] Client disconnect.");
			s->Close();
			break;
		}

		addMessageToChatWindow(D3DCOLOR_XRGB(0xFF, 0xFF, 0xFF), r.c_str());
	}

	delete s;

	return 0;
}

void SockThread()
{
	while (true)
	{
		if (sockState == 0)
		{
			try 
			{
				SocketClient s("127.0.0.1", 9876);
				addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMITTER] Connected");

				while (true)
				{
					while (!chat.empty())
					{
						if (sockState != 0)
						{
							break;
						}

						s.SendLine(chat.front().c_str());
						chat.pop();
						Sleep(500);
					}
					if (sockState != 0)
					{
						break;
					}
				}

				s.Close();
			}
			catch (const char* s) 
			{
				addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMITTER] Socket err: %s", s);
				Sleep(10000);
			}
			catch (std::string s) 
			{
				addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMITTER] Socket err: %s", s.c_str());
				Sleep(10000);
			}
			catch (...) 
			{
				addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMITTER] Socket exception");
				Sleep(10000);
			}
		}
		else if (sockState == 1)
		{
			SocketServer in(8346, 1);

			while (true)
			{
				if (sockState != 1)
				{
					in.Close();
					break;
				}

				Socket* s = in.Accept();

				addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "[CHAT EMITTER] Client connection.");

				unsigned ret;
				_beginthreadex(0, 0, ClientRecvThr, (void*)s, 0, &ret);
			}
			in.Close();
		}
	}
	return SockThread();
}


BOOL WINAPI DllMain(
	HINSTANCE hinstDLL, 
	DWORD fdwReason,    
	LPVOID lpReserved) 
{

	g_DllHandle = hinstDLL;

	DisableThreadLibraryCalls((HMODULE)hinstDLL);

	if (fdwReason != DLL_PROCESS_ATTACH)
		return FALSE;

	if (GetModuleHandle("samp.dll"))
	{
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nnMain, NULL, 0, NULL);
	}

	return TRUE;  
}