#include "global.h"
#include "playtime.h"
#include "connect.h"
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <Windows.h>
#pragma comment(lib,"ws2_32.lib")

extern HANDLE sOut;
extern unsigned char conblock[WIDTH][HEIGTH];
extern unsigned short curblock, nextblock;
extern unsigned char blockcolor, nextblockcolor;
extern unsigned int blocktime;
extern unsigned int grade;
extern time_t starttime;

static ULONGLONG rectimer;

void Dou_Init();
int Con_Start();
void repaint();
int abletoset(unsigned short block, COORD blockpos);
void layblock();
int UserOP2(INPUT_RECORD inp);
void repaint2(char*);
void dou_sendcon();
void spawnrand();
void flushfullline(BOOL spe);
BOOL isContinue(unsigned short block, COORD blockpos);

int dou_play() {
	WSADATA wdata;
	if (WSAStartup(MAKEWORD(2, 2), &wdata) == SOCKET_ERROR) {
		return -1;
	}
	if (HIBYTE(wdata.wVersion) != 2 || LOBYTE(wdata.wVersion) != 2) {
		return -1;
	}
	Con_Start();
	WSACleanup();
	FitScreen((COORD) { 2 * WIDTH + 6, HEIGTH + 3 },hOut);
	return 0;
}

BOOL dou_deal(INPUT_RECORD inp,unsigned int runstate) {
	if (runstate == RUNSTATE_ACCEPT) {
		if (inp.EventType != KEY_EVENT) return TRUE;
		if (inp.Event.KeyEvent.uChar.UnicodeChar == 'q') return FALSE;
	}
	if (runstate == RUNSTATE_CONNECTED) {
		UserOP2(inp);
	}
	return TRUE;
}

BOOL dou_msg(char* msg, unsigned int runstate) {
	if (runstate != RUNSTATE_CONNECTED) return TRUE;
	if (!strncmp(msg, "blocks:", strlen("blocks:"))) {
		repaint2(msg);
	}
	else if (!strncmp(msg, "gameover:", strlen("gameover:"))) {

		return FALSE;
	}
	return TRUE;
}

BOOL dou_timer(BOOL isFirst) {
	if (isFirst) {
		Dou_Init();
		spawnrand();
		rectimer = GetTickCount64();
		FitScreen((COORD) { 4 * WIDTH + 12, HEIGTH + 3 },hOut);
		return TRUE;
	}
	if (GetTickCount64() >= rectimer + SLEEPCLOCK) {
		if (abletoset(curblock, (COORD) { blockpos.X, blockpos.Y + 1 })) {
			blockpos.Y++; 
		}
		else {
			if (!isContinue(curblock,blockpos)) {
				send(sock, "gameover:", strlen("gameover:"), 0);
				return FALSE;
			}
			layblock();
			spawnrand();
			flushfullline(TRUE);
		}
		repaint2(recvmsg);
		dou_sendcon();
		rectimer = GetTickCount64();
	}
	return TRUE;
}

int Con_Start() {
	ClearScr(hOut);
	WCHAR wch[16] = { 0 }; DWORD recnum;
	char readip[16] = { 0 };
	struct in_addr** addrins = GetLocalIP(),**saddrins = addrins;
	if (!*addrins) {
		WriteConsoleW(hOut, L"并没有检测到网络,请检测防火墙或者网络设置\n任意键继续...",wcslen(L"并没有检测到网络,请检测防火墙或者网络设置\n任意键继续..."),&recnum,NULL);
		if (_getch());
		return 1;
	}
	WriteConsole(hOut, L"你的局域网IP:\n", wcslen(L"你的局域网IP:\n"), &recnum, NULL);
	do {
		wsprintf(wch, L"%d.%d.%d.%d\n", (**addrins).S_un.S_un_b.s_b1, (**addrins).S_un.S_un_b.s_b2, (**addrins).S_un.S_un_b.s_b3, (**addrins).S_un.S_un_b.s_b4);
		SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		WriteConsole(hOut, wch, wcslen(wch), &recnum, NULL);
	} while (*++addrins);
	WriteConsole(hOut, L"\n请输入连接IP地址\n(如果是服务器则无需输入,退出请输入exit)\n>", wcslen(L"\n请输入连接IP地址\n(如果是服务器则无需输入,退出请输入exit)\n>"), &recnum, NULL);
	while (1) {
		fgets(readip, 16, stdin);
		if (readip[15] && readip[15] != '\n') {
			puts("请输入正确的IP地址");
			continue;
		}
		struct in_addr addr;
		if (readip[0] == '\n') {
			puts("连接中...");
			spawnrand();
			addr = **saddrins;
			int ret = connect_player(TRUE, addr, dou_msg, dou_deal, dou_timer);
			if (ret) {
				printf("错误发生:%d\n任意键退出", ret);
				if (_getch());
			}
			break;
		}
		if (!strcmp(readip, "exit\n")) break;
		unsigned short ipaddr[4];
		if (sscanf_s(readip, "%hu.%hu.%hu.%hu\n", ipaddr,&ipaddr[1], &ipaddr[2], &ipaddr[3]) != 4) {
			puts("格式化失败");
			continue;
		}
		if ((ipaddr[0] & 0xff00) || (ipaddr[1] & 0xff00) || (ipaddr[2] & 0xff00) || (ipaddr[3] & 0xff00)) {
			puts("格式化失败");
			continue;
		}
		addr.S_un.S_un_b.s_b1 = (BYTE)ipaddr[0];
		addr.S_un.S_un_b.s_b2 = (BYTE)ipaddr[1];
		addr.S_un.S_un_b.s_b3 = (BYTE)ipaddr[2];
		addr.S_un.S_un_b.s_b4 = (BYTE)ipaddr[3];
		puts("连接中(按q退出连接)");
		spawnrand();
		int ret = connect_player(FALSE, addr, dou_msg, dou_deal,dou_timer);
		if (ret) {
			printf("错误发生:%d\n任意键退出", ret);
			if (_getch());
			break;
		}
		return 0;
	}
	return 1;
}

void Dou_Init() {
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleScreenBufferSize(hOut, (COORD) { 4 * WIDTH + 12, HEIGTH + 3 });
	SetConsoleWindowInfo(hOut, TRUE, &(SMALL_RECT){0, 0, 4 * WIDTH + 11, HEIGTH + 2});
	SetConsoleScreenBufferSize(hOut, (COORD) { 4 * WIDTH + 12, HEIGTH + 3 });

	ClearScr(hOut);

	sOut = CreateConsoleScreenBuffer(GENERIC_WRITE|GENERIC_READ, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	if (sOut == INVALID_HANDLE_VALUE) {
		puts("CreateConsoleScreenBuffer Function Failed");
		exit(1);
	}
	//SetConsoleWindowInfo(sOut, TRUE, &(SMALL_RECT){0, 0, 4 * WIDTH + 11, HEIGTH + 2});
	SetConsoleScreenBufferSize(sOut, (COORD) { 4 * WIDTH + 12, HEIGTH + 3 });
	SetConsoleCursorInfo(sOut, &(CONSOLE_CURSOR_INFO){1, 0});
	repaint();
}

int UserOP2(INPUT_RECORD inp) {
	if (inp.EventType != KEY_EVENT) return TRUE;
	if (!inp.Event.KeyEvent.bKeyDown) return TRUE;
	unsigned short block;
	switch (inp.Event.KeyEvent.wVirtualKeyCode) {
	case VK_UP:
		if (abletoset(block = aclockwise(curblock, 1), blockpos)) {
			curblock = block;
			repaint2(recvmsg);
			dou_sendcon();
			return 1;
		}
		break;
	case VK_DOWN:
		if (abletoset(curblock, (COORD) { blockpos.X, blockpos.Y + 1 })) {
			blockpos.Y++; repaint2(recvmsg);
			dou_sendcon(); return 1;
		}
		else {
			layblock(); spawnrand(); flushfullline(TRUE);
			repaint2(recvmsg);
			dou_sendcon(); 
			return 1;
		}
		break;
	case VK_LEFT:
		if (abletoset(curblock, (COORD) { blockpos.X - 1, blockpos.Y })) {
			blockpos.X--;
			repaint2(recvmsg);
			dou_sendcon();
			return 1;
		}
		break;
	case VK_RIGHT:
		if (abletoset(curblock, (COORD) { blockpos.X + 1, blockpos.Y })) {
			blockpos.X++;
			repaint2(recvmsg);
			dou_sendcon();
			return 1;
		}
		break;
	case VK_SPACE:
		while (abletoset(curblock, (COORD) { blockpos.X, ++blockpos.Y })); --blockpos.Y;
		layblock(); spawnrand(); flushfullline(TRUE); repaint2(recvmsg);
		dou_sendcon();
		break;
	}
	return 0;
}
void repaint2(char *msg) {
	msg += 7;
	int num = WIDTH,recnum,num2;
	SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 5, 0 });
	SetConsoleTextAttribute(sOut,SCOLOR_FORM);
	if (isWinUpper) {
		WriteConsole(sOut, L"┌", 2, &recnum, NULL);
		while (num--) WriteConsole(sOut, L"─", 2, &recnum, NULL);
		WriteConsole(sOut, L"┐", 2, &recnum, NULL);
		for (num2 = 0; num2 < HEIGTH; num2++) {
			SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 5, num2 + 1 });
			WriteConsole(sOut, L"│", 2, &recnum, NULL);
			for (num = 0; num < WIDTH; num++) {
				unsigned int paintblock;
				sscanf_s(msg, "%2x", &paintblock);
				msg += 2;
				if (paintblock) {
					SetConsoleTextAttribute(sOut, paintblock);
					WriteConsole(sOut, L"□", 1, &recnum, NULL);
				}
				else {
					SetConsoleTextAttribute(sOut, SCOLOR_BACKGROUND);
					WriteConsole(sOut, L"  ", 2, &recnum, NULL);
				}
			}
			SetConsoleTextAttribute(sOut, SCOLOR_FORM);
			WriteConsole(sOut, L"│", 2, &recnum, NULL);
		}
		SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 5, HEIGTH + 1 });
		WriteConsole(sOut, L"└", 2, &recnum, NULL);
		num = WIDTH;
		while (num--) WriteConsole(sOut, L"─", 2, &recnum, NULL);
		WriteConsole(sOut, L"┘", 2, &recnum, NULL);
		int recvblock, tmpblockx, tmpblocky,recvcolor;
		sscanf_s(msg, "%8x%8x%8x%8x", &recvblock, &tmpblockx, &tmpblocky,&recvcolor);
		short blockx = (short)tmpblockx, blocky = (short)tmpblocky;
		SetConsoleTextAttribute(sOut, recvcolor);
		for (num2 = blocky + 3; num2 >= blocky; num2--) {
			if (num2 < 0) break;
			for (num = blockx + 3; num >= blockx; num--) {
				if (num < 0) break;
				if (recvblock & ((0x8000 >> ((num2 - blocky) * 4) >> (num - blockx)))) {
					SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 7 + 2 * num, num2 + 1 });
					WriteConsole(sOut, L"□", 1, &recnum, NULL);
				}
			}
		}
	}
	else {
		WriteConsole(sOut, L"┌", 1, &recnum, NULL);
		while (num--) WriteConsole(sOut, L"─", 1, &recnum, NULL);
		WriteConsole(sOut, L"┐", 1, &recnum, NULL);
		for (num2 = 0; num2 < HEIGTH; num++) {
			SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 5, num2 + 1 });
			WriteConsole(sOut, L"│", 1, &recnum, NULL);
			for (num = 0; num < WIDTH; num++) {
				unsigned int paintblock;
				sscanf_s(msg, "%2x", &paintblock);
				msg += 2;
				if (paintblock) {
					SetConsoleTextAttribute(sOut, paintblock);
					WriteConsole(sOut, L"□", 1, &recnum, NULL);
				}
				else {
					SetConsoleTextAttribute(sOut, SCOLOR_BACKGROUND);
					WriteConsole(sOut, L"  ", 2, &recnum, NULL);
				}
			}
			WriteConsole(sOut, L"│", 1, &recnum, NULL);
		}
		SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 5, HEIGTH + 1 });
		WriteConsole(sOut, L"└", 1, &recnum, NULL);
		num = WIDTH;
		while (num--) WriteConsole(sOut, L"─", 1, &recnum, NULL);
		WriteConsole(sOut, L"┘", 1, &recnum, NULL);
		int recvblock, tmpblockx, tmpblocky, recvcolor;
		sscanf_s(msg, "%8x%8x%8x%8x", &recvblock, &tmpblockx, &tmpblocky, &recvcolor);
		SetConsoleTextAttribute(sOut, recvcolor);
		short blockx = (short)tmpblockx, blocky = (short)tmpblocky;
		for (num2 = blocky + 3; num2 >= blocky; num2--) {
			if (blocky < 0) break;
			for (num = blockx + 3; num >= blockx; num--) {
				if (num < 0) break;
				if (recvblock & ((0x8000 >> ((num2 - blocky) * 4) >> (num - blockx)))) {
					SetConsoleCursorPosition(sOut, (COORD) { 2 * WIDTH + 7 + 2 * num, num2 + 1 });
					WriteConsole(sOut, L"□", 1, &recnum, NULL);
				}
			}
		}
	}
	repaint();
	CHAR_INFO wstr[(4 * WIDTH + 12) * (HEIGTH + 3)];
	ReadConsoleOutput(hOut, wstr, (COORD) { 4 * WIDTH + 12, HEIGTH + 3 }, (COORD) { 0, 0 }, & (SMALL_RECT){0, 0, 4 * WIDTH + 11, HEIGTH + 2});
	WriteConsoleOutput(sOut, wstr, (COORD) { 4 * WIDTH + 12, HEIGTH + 3 }, (COORD) { 0, 0 }, & (SMALL_RECT){0, 0, 4 * WIDTH + 11, HEIGTH + 2});
	//SetConsoleActiveScreenBuffer(sOut);
	//FlushFileBuffers(sOut);
	//exchange(hOut, sOut, HANDLE);
}
void dou_sendcon() {
	char msg[MSG_LEN + 1] = "blocks:", * tmp = msg;
	unsigned short x, y;
	while (*++tmp);
	for (y = 0; y < HEIGTH; y++) {
		for (x = 0; x < WIDTH; x++) {
			sprintf_s(tmp, 3, "%02x", conblock[x][y]);
			tmp += 2;
		}
	}
	sprintf_s(tmp, 33, "%08x%08x%08x%08x", (unsigned)curblock, (int)blockpos.X, (int)blockpos.Y, (unsigned)blockcolor);
	send(sock, msg, MSG_LEN, 0);
}