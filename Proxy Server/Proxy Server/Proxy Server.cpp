#include "stdafx.h"
#include "Proxy Server.h"
#include "afxsock.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define HTTP "http://"
#define PORT 8888
#define HTTPPORT 80
#define SIZE 20000

CWinApp theApp;
using namespace std;
int global = 0;
//struct chứa thông tin của Client và Server 
struct SocketCap
{
	SOCKET Server;
	SOCKET Client;
	bool ServerClose;
	bool ClientClose;
};
//struct chứa các thông tin Query
struct ThongTin
{
	HANDLE h;
	string address;
	SocketCap *Cap;
	int port;
};

//Hàm khởi tạo Server 
void StartServer();
//Thread giao tiếp giữa Client và Proxy Server
UINT Client_Proxy(void *lParam);
//Thread giao tiếp giữa Proxy Server và Remote Server
UINT Proxy_Server(void *lParam);
//Đóng Server
void CloseServer();
//Nhận input từ console, nhập T để dừng Server
UINT NhanThongSo(void *thongso);
//Lấy địa chỉ, port và tạo truy vấn từ truy vấn nhận được từ Client
void GetAddrNPort(string &buf, string &address, int &port);
//Tách chuỗi
void TachChuoi(string str, vector<string> &cont, char delim = ' ');
//Refhttp://stackoverflow.com/questions/19715144/how-to-convert-char-to-lpcwstr
//Chuyển char array sang dạng (chuỗi kí tữ unicode)
wchar_t *ChuyenChar_Unicode(const char* charArray);
//Load file blacklist.conf
void BlackList(vector<string> &arr);
//Kiểm tra xem địa chỉ có nằm trong black list hay không
bool Kiem_Tra_Server_Name(string server_name);
//Lấy địa chỉ IP để yêu cầu kết nối
//Cấu trúc SOCKADDR_IN chỉ định địa chỉ vận chuyển và cổng cho họ địa chỉ AF_INET .
sockaddr_in* GetServer(string server_name, char*hostname);

typedef SOCKET Socket;
vector<string> black_list;
//Chuỗi trả về khi nằm trong black list
string ResForbidden = "HTTP/1.0 403 Forbidden";
//Socket dùng để lắng nghe các truy cập mới
Socket NewListen;
bool run = 1;

void StartServer()
{
	sockaddr_in local;
	Socket listen_socket;
	WSADATA wsaData;
	//Cài đặt các Socket
	/*int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);

	Trong đó :
	-wVersionRequested là phiên bản thư viện mà mình sử dụng.Ở đây sẽ là giá trị 0x0202 có nghĩa là phiên bản 2.2.Chúng ta có thể dùng macro MAKEWORD(2, 2) để trả về giá trị 0x0202.
	- lpWSData là một số thông  tinbổ sung sẽ được trả về sau khi gọi khởi tạo Winsock. :
	*/
	if (WSAStartup(0x202, &wsaData) != 0)
	{
		cout << "\nLOI KHOI TAO SOCKET\n";
		WSACleanup();  //hàm hủy Winsock khi kết thúc chương trình.

		return;
	}
	local.sin_family = AF_INET;//(sin_family)dạng protocol của socket
	local.sin_addr.s_addr = INADDR_ANY;  //địa chỉ internet của socket
	local.sin_port = htons(PORT);//chỉ số port
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	//Khỏi tạo socket
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "\nsSOCKET KHOI TAO LOI.";
		WSACleanup();
		return;
	}
	//Bind Socket 
	if (bind(listen_socket, (sockaddr *)&local, sizeof(local)) != 0)
	{
		cout << "\n Loi khi bind socket.";      //gán số hiệu port cho socket.
		WSACleanup();
		return;
	};
	//Bắt đầu lắng nghe các truy cập
	if (listen(listen_socket, 5) != 0)
	{
		cout << "\n LOI NGHE.";
		WSACleanup();
		return;
	}
	BlackList(black_list);
	if (black_list.size() == 0)
	{
		cout << "FILE BLACKLIST TRONG." << endl;
	}
	//Socket dùng để lắng nghe các truy cập mới
	NewListen = listen_socket;
	//Bắt đầu thread giao tiếp giữa Client và Proxy Server
	AfxBeginThread(Client_Proxy, (LPVOID)listen_socket);
	//Bắt đầu thread đợi input console để dừng 
	CWinThread* p = AfxBeginThread(NhanThongSo, &run);
}
//Thread giao tiếp giữa Client và Proxy Server
UINT Client_Proxy(void * lParam)
{
	Socket socket = (Socket)lParam;
	SocketCap Cap;
	SOCKET SClient;
	sockaddr_in addr;
	int addrLen = sizeof(addr);
	//Truy cập mới
	SClient = accept(socket, (sockaddr*)&addr, &addrLen);
	//Khỏi tạo một thread khác để tiếp tục lắng nghe
	AfxBeginThread(Client_Proxy, lParam);
	char Dem[SIZE];   //biến đệm
	int Len;
	Cap.ServerClose = FALSE;
	Cap.ClientClose= FALSE;
	Cap.Client = SClient;
	//Nhận truy vấn gởi tới từ Client
	int returnvalue = recv(Cap.Client, Dem, SIZE, 0);
	if (returnvalue == SOCKET_ERROR)
	{
		cout << "\nloi khi nhan yeu cau" << endl;
		if (!Cap.ClientClose)
		{
			closesocket(Cap.Client);
			Cap.ClientClose = TRUE;
		}
	}
	if (returnvalue == 0)
	{
		cout << "\nclient ngat ket noi" << endl;
		if (!Cap.ClientClose)
		{
			closesocket(Cap.Client);
			Cap.ClientClose = TRUE;
		}
	}
	
	if (returnvalue >= SIZE)
	{
		Dem[returnvalue - 1] = 0;
	}
	else
	{
		if (returnvalue > 0)
		{
			Dem[returnvalue] = 0;
		}
		else
		{
			Dem[0] = 0;
		}

	}
	//xuất truy vấn ra
	cout << "\n Client da nhan: " << returnvalue << "data :\n[" << Dem << "]";
	string buf(Dem), address;
	int port;
	GetAddrNPort(buf, address, port);
	bool check = FALSE;
	if (!Kiem_Tra_Server_Name(address))
	{
	//Nếu địa chỉ nằm trong black list sẽ trả về Error 403 mà không phải gởi truy vấn tới remote server
		returnvalue = send(Cap.Client, ResForbidden.c_str(), ResForbidden.size(), 0);
		check = TRUE;
		Sleep(2000);
	}
	ThongTin P;
	P.h = CreateEvent(NULL, TRUE, FALSE, NULL);
	P.address = address;
	P.port = port;
	P.Cap = &Cap;
	if (check == FALSE)
	{
		//Bắt đầu thread giao tiếp giữa Proxy Server và Remote Server
		CWinThread* pThread = AfxBeginThread(Proxy_Server, (LPVOID)&P);
		//Đợi cho Proxy kết nối với Server
		WaitForSingleObject(P.h, 6000);
		CloseHandle(P.h);
		while (Cap.ClientClose == FALSE && Cap.ServerClose == FALSE)
		{
			//Proxy gởi truy vấn
			returnvalue = send(Cap.Server, buf.c_str(), buf.size(), 0);
			if (returnvalue == SOCKET_ERROR)
			{
				cout << "Gui khong thanh cong." << GetLastError();
				if (Cap.ServerClose == FALSE)
				{
					closesocket(Cap.Server);
					Cap.ServerClose= TRUE;
				}
				continue;
			}
			//Tiếp tục nhận các truy vấn từ Client
			//Vòng lặp này sẽ chạy đến khi nhận hết data, 1 trong 2 Client và Server sẽ ngắt kết nối
			returnvalue = recv(Cap.Client, Dem, SIZE, 0);
			if (returnvalue == SOCKET_ERROR)
			{
				cout << "C Receive Failed, Error: " << GetLastError();
				if (Cap.ClientClose== FALSE)
				{
					closesocket(Cap.Client);
					Cap.ClientClose = TRUE;
				}
				continue;
			}
			if (returnvalue == 0)
			{
				cout << "Client closed " << endl;
				if (Cap.ClientClose== FALSE)
				{
					closesocket(Cap.Server);
					Cap.ClientClose = TRUE;
				}
				break;
			}
			
			if (returnvalue >= SIZE)
			{
				Dem[returnvalue - 1] = 0;
			}
			else
			{
				if (returnvalue > 0)
				{
					Dem[returnvalue] = 0;
				}
				else
				{
					Dem[0] = 0;
				}
			}
			cout << "\n Client received " << returnvalue << "data :\n[" << Dem << "]";
		}
		if (Cap.ServerClose == FALSE)
		{
			closesocket(Cap.Server);
			Cap.ServerClose = TRUE;
		}
		if (Cap.ClientClose == FALSE)
		{
			closesocket(Cap.Client);
			Cap.ClientClose= TRUE;
		}
		WaitForSingleObject(pThread->m_hThread, 20000);
	}
	else
	{
		if (Cap.ClientClose == FALSE)
		{
			closesocket(Cap.Client);
			Cap.ClientClose = TRUE;
		}
	}
	return 0;
}

//Thread giao tiếp giữa Proxy Server và Remote Server
UINT Proxy_Server(void * lParam)
{
	int count = 0;
	ThongTin *P = (ThongTin*)lParam;
	string server_name = P->address;
	int port = P->port;
	int status;
	int addr;
	char hostname[32] = "";
	sockaddr_in *server = NULL;
	cout << "Server Name: " << server_name << endl;
	server = GetServer(server_name, hostname);
	if (server == NULL)
	{
		cout << "\n Khong the lay dia chi IP" << endl;
		send(P->Cap->Client, ResForbidden.c_str(), ResForbidden.size(), 0);
		return -1;
	}
	if (strlen(hostname) > 0)
	{
		cout << "KET NOI TOI:" << hostname << endl;
		int returnvalue;
		char Dem[SIZE];
		Socket Server;
		Server = socket(AF_INET, SOCK_STREAM, 0);
		//Kết nối tới địa chỉ IP vừa lấy được
		if (!(connect(Server, (sockaddr*)server, sizeof(sockaddr)) == 0))
		{
			cout << "KHONG THE KET NOI";
			send(P->Cap->Client, ResForbidden.c_str(), ResForbidden.size(), 0);

			return -1;
		}
		else
		{
			cout << "KET NOI THANH CONG \n";
			P->Cap->Server = Server;
			P->Cap->ServerClose == FALSE;
			SetEvent(P->h);
			int c = 0;
			while (P->Cap->ClientClose== FALSE &&
				P->Cap->ServerClose == FALSE)
			{
				//Nhận data gởi từ Server tới Proxy
				returnvalue = recv(P->Cap->Server, Dem, SIZE, 0);
				if (returnvalue == SOCKET_ERROR)
				{
					closesocket(P->Cap->Server);
					P->Cap->ServerClose = TRUE;
					break;
				}
				if (returnvalue == 0)
				{
					cout << "\nServer Closed" << endl;
					closesocket(P->Cap->Server);
					P->Cap->ServerClose = TRUE;
				}
				//Gởi data đó tới Client
				//Kết thúc vòng lặp khi đã nhận và gởi hết data
				returnvalue = send(P->Cap->Client, Dem, returnvalue, 0);
				if (returnvalue == SOCKET_ERROR)
				{
					cout << "\nGui khong thanh cong, Error: " << GetLastError();
					closesocket(P->Cap->Client);
					P->Cap->ClientClose = TRUE;
					break;
				}
				
				if (returnvalue >= SIZE)
				{
					Dem[returnvalue - 1] = 0;
				}
				else
				{
					Dem[returnvalue] = 0;
				}
				cout << "\n Server da nhan: " << returnvalue << "data :\n[" << Dem << "]";
				ZeroMemory(Dem, SIZE);
			}
			//Đóng socket
			//Việc thay đổi giá trị ở thread này sẽ ảnh hưởng tới thread ClientToProxy
			//Việc đóng Socket ở thread này => các giá trị thread kia cũng thay đổi
			if (P->Cap->ClientClose == FALSE)
			{
				closesocket(P->Cap->Client);
				P->Cap->ClientClose= TRUE;
			}
			if (P->Cap->ServerClose == FALSE)
			{
				closesocket(P->Cap->Server);
				P->Cap->ServerClose = TRUE;
			}
		}
	}

	return 0;
}

void CloseServer()
{
	//Đóng Socket lắng nghe
	cout << "Dong Socket" << endl;
	closesocket(NewListen);
	WSACleanup();
}

//Nhận thông số từ console, nhập T để dừng Server
UINT NhanThongSo(void * thongso)
{
	bool * r = (bool*)thongso;
	while (*r)
	{
		char c;
		c = getchar();
		if (c == 'T')
		{
			*r = 0;
		}
	}
	return 0;
}

//Lấy địa chỉ, port và tạo truy vấn từ truy vấn nhận được từ Client
void GetAddrNPort(string &buf, string &address, int &port)
{
	vector<string> cont;
	//cont 0: command, 1: link, 2: proto
	TachChuoi(buf, cont);
	if (cont.size() > 0) 
	{
		int pos = cont[1].find(HTTP);
		if (pos != -1)
		{
			string add = cont[1].substr(pos + strlen(HTTP));
			address = add.substr(0, add.find('/'));
			//Port của HTTP là 80
			port = 80;
			string temp;
			int len = strlen(HTTP) + address.length();
			while (len > 0) 
			{
				temp.push_back(' ');
				len--;
			}
			buf = buf.replace(buf.find(HTTP + address), strlen(HTTP) + address.length(), temp);
		}
	}
}

//Tách chuỗi
void TachChuoi(string str, vector<string> &cont, char delim)
{
	istringstream ss(str);
	string x;
	while (getline(ss, x, delim))
	{
		cont.push_back(x);
	}
}

//Chuyển char array sang dạng LPCWSTR(chuỗi kí tữ unicode)
wchar_t *ChuyenChar_Unicode(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

//Load file blacklist.conf
void BlackList(vector<string>& arr)
{
	fstream f;
	f.open("blacklist.conf", ios::in | ios::out);
	if (f.is_open())
	{
		while (!f.eof())
		{
			string temp;
			getline(f, temp);
			if (temp.back() == '\n')
			{
				temp.pop_back();
			}
			arr.push_back(temp);
		}
	}
}

//Kiểm tra xem địa chỉ có nằm trong black list hay không
bool Kiem_Tra_Server_Name(string server_name)
{
	if (black_list.size() > 0)
	{
		for (auto i : black_list)
		{
			if (i.find(server_name) != string::npos)
			{
				cout << i.find(server_name);
				return 0;
			}
		}
	}
	return 1;
}
//Lấy địa chỉ IP để yêu cầu kết nối
//Cấu trúc SOCKADDR_IN chỉ định địa chỉ vận chuyển và cổng cho họ địa chỉ AF_INET
sockaddr_in* GetServer(string server_name, char * hostname)
{
	int trangthai;
	sockaddr_in *server = NULL;
	if (server_name.size() > 0)
	{
		//Kiểm tra xem địa chỉ lấy được ở địa dạng nào TÊN MIỀN hay IP
		//Isalpha kiểm tra xem kí tự truyền có phải chữ cái hay không
		if (isalpha(server_name.at(0)))
		{
			//Các addrinfo cấu trúc:chức năng để giữ thông tin địa chỉ host.
			addrinfo x, *res = NULL;
			ZeroMemory(&x, sizeof(x));
			x.ai_family = AF_UNSPEC;
			x.ai_socktype = SOCK_STREAM;
			//Lấy thông tin từ địa chỉ lấy được 
			if ((trangthai = getaddrinfo(server_name.c_str(), "80", &x, &res)) != 0)
			{
				printf("getaddrinfo failed: %s", gai_strerror(trangthai));
				return NULL;
			}
			while (res->ai_next != NULL)
			{
				res = res->ai_next;
			}
			sockaddr_in * temp = (sockaddr_in*)res->ai_addr;
			inet_ntop(res->ai_family, &temp->sin_addr, hostname, 32);
			server = (sockaddr_in*)res->ai_addr;
			unsigned long addr;
			inet_pton(AF_INET, hostname, &addr);
			server->sin_addr.s_addr = addr;
			server->sin_port = htons(80);
			server->sin_family = AF_INET;
		}
	    
		else
		{
			unsigned long addr;
			inet_pton(AF_INET, server_name.c_str(), &addr);
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = addr;
			if ((trangthai = getnameinfo((sockaddr*)&sa,
				sizeof(sockaddr), hostname, NI_MAXHOST, NULL, NI_MAXSERV, NI_NUMERICSERV)) != 0)
			{
				cout << " LOI ";
				return NULL;
			}
			server->sin_addr.s_addr = addr;
			server->sin_family = AF_INET;
			server->sin_port = htons(80);
		}
	}
	
	return server;
}

int main()
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			wprintf(L"Fatal Error: MFC initialization failed\n");
			nRetCode = 1;
		}
		else
		{

			StartServer();
			while (run)
			{
				Sleep(1000);
			}
			CloseServer();
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		wprintf(L"Fatal Error: GetModuleHandle failed\n");
		nRetCode = 1;
	}

	return nRetCode;
}
