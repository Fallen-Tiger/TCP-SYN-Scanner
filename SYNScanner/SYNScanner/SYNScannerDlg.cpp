
// SYNScannerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SYNScanner.h"
#include "SYNScannerDlg.h"
#include "afxdialogex.h"
#include "SYNSend.h"
#include <Mstcpip.h>

#define RECV_BUF_SIZE	TCP_WINDOW_SIZE
#define TIMER_ID_RECV_TIMEOUT	5174

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSYNScannerDlg dialog



CSYNScannerDlg::CSYNScannerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SYNSCANNER_DIALOG, pParent), m_sockRawSend(INVALID_SOCKET), m_sockRawRecv(INVALID_SOCKET), m_hThreadSend(NULL), m_hThreadRecv(NULL)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSYNScannerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_IPADDRESS_DST_IP, m_ipaddrDst);
	DDX_Control(pDX, IDC_IPADDRESS_SRC_IP, m_ipaddrSrc);
	DDX_Control(pDX, IDC_EDIT_DST_START_PORT, m_editDstStartPort);
	DDX_Control(pDX, IDC_EDIT_DST_END_PORT, m_editDstEndPort);
	DDX_Control(pDX, IDC_EDIT_SRC_PORT, m_editSrcPort);
	DDX_Control(pDX, IDC_LIST_SCAN_RESULTS, m_clistctrlScanResults);
	DDX_Control(pDX, IDC_EDIT_RECV_TIME_OUT, m_editRecvTimeOut);
	DDX_Control(pDX, IDC_LIST_VALID_PORTS, m_clistctrlValidPorts);
	DDX_Control(pDX, IDC_BUTTON_SCAN, m_cbtnScan);
}

BEGIN_MESSAGE_MAP(CSYNScannerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_SCAN, &CSYNScannerDlg::OnClickedButtonScan)
	ON_WM_CLOSE()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CSYNScannerDlg message handlers

BOOL CSYNScannerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	//Judge whether the system support raw tcp
	/*if (!IsWindowsServer()) {
		CString str;
		str.Format(TEXT("Can't running at your system version.Windows Server run correctly"));
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
	}*/
	//Initailize WIN SOCKET
	int iResult = 0;
	WSADATA wsaData = { 0 };
	iResult = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("WSAStartup failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
		return TRUE;
	}
	//Get My IP address
	char strHostName[255] = {0};
	iResult = gethostname(strHostName, 255);
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("gethostname failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
		return TRUE;
	}
	PHOSTENT pHostInfo = NULL;
	pHostInfo = gethostbyname(strHostName);
	if (pHostInfo == NULL) {
		CString str;
		str.Format(TEXT("gethostbyname failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
		return TRUE;
	}
	m_cstrMyIP = inet_ntoa(*(struct in_addr *)pHostInfo->h_addr_list[0]);
	
	//Create raw socket
	iResult = m_sockRawSend = socket(AF_INET, SOCK_RAW, IPPROTO_IP);
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("socket failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
		return TRUE;
	}

	//set IP_HDRINCL to socket for modify raw ip data by yourself
	DWORD  bRaw = TRUE;
	iResult = setsockopt(m_sockRawSend, IPPROTO_IP, IP_HDRINCL, (const char *)&bRaw, sizeof(bRaw));
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("setsockopt IP_HDRINCL failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		SendMessage(WM_CLOSE);
		return TRUE;
	}

	//Set my ip to ip addr control
	m_ipaddrSrc.SetAddress((DWORD)htonl(*(u_long*)pHostInfo->h_addr_list[0]));
	m_editSrcPort.SetLimitText(5);
	m_editDstEndPort.SetLimitText(5);
	m_editDstStartPort.SetLimitText(5);
	m_editRecvTimeOut.SetWindowTextW(TEXT("2000"));//Set initial time out
	m_editSrcPort.SetWindowTextW(TEXT("5174"));//Set initial source port

	m_clistctrlScanResults.InsertColumn(0, TEXT("IP ADDRESS"), LVCFMT_LEFT, 100);
	m_clistctrlScanResults.InsertColumn(1, TEXT("DST PORT"), LVCFMT_LEFT, 100);
	m_clistctrlScanResults.InsertColumn(2, TEXT("SYN SEND STATUS"), LVCFMT_LEFT, 150);
	//m_clistctrlScanResults.InsertColumn(3, TEXT("IS LISTENING"), LVCFMT_LEFT, 150);

	m_clistctrlValidPorts.InsertColumn(0, TEXT("OPEN PROT"), LVCFMT_LEFT, 100);

	GetWindowText(m_cstrWindowsText);
	//used to generate random identification
	srand(time(NULL));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSYNScannerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSYNScannerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSYNScannerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

DWORD WINAPI ThreadProcSend(
	_In_ LPVOID lpParameter
) {
	CSYNScannerDlg * pWnd = (CSYNScannerDlg *)lpParameter;
	
	DWORD dwHostIPSrc = 0,
		dwHostIPDst = 0;
	UINT16 usHostSrcPort = 0,
		usHostDstStartPort = 0,
		usHostDstEndPort = 0;
	CString cstrTemp;

	pWnd->m_ipaddrSrc.GetAddress(dwHostIPSrc);
	pWnd->m_ipaddrDst.GetAddress(dwHostIPDst);

	pWnd->m_editSrcPort.GetWindowTextW(cstrTemp);
	usHostSrcPort = _ttoi(cstrTemp);
	pWnd->m_editDstStartPort.GetWindowTextW(cstrTemp);
	usHostDstStartPort = _ttoi(cstrTemp);
	pWnd->m_editDstEndPort.GetWindowTextW(cstrTemp);
	usHostDstEndPort = _ttoi(cstrTemp);

	sockaddr_in addrRecver = { 0 };
	addrRecver.sin_family = AF_INET;
	addrRecver.sin_addr.S_un.S_addr = htonl(dwHostIPDst);

	int iResult = 0;
	bool bSended = 0;
	u_long ulnDstIP = htonl(dwHostIPDst);
	CString cstrDstIP(inet_ntoa(*(in_addr *)&ulnDstIP));//Get IP Addr
	CString cstrPortTemp;
	UINT16 usOffset = (UINT16)rand();//to generate identification
	for (UINT16 iDstPort = usHostDstStartPort; iDstPort <= usHostDstEndPort; iDstPort++) {
		bSended = true;
		//create ip tcp packets 
		IP_HEADER ipHeader = { 0 };
		TCP_HEADER tcpHeader = { 0 };
		
		ipHeader = getIpHeader(dwHostIPSrc, dwHostIPDst, usOffset + iDstPort); //identification = 4037 + destination prot
		tcpHeader = getTCPHeader(&ipHeader, usHostSrcPort, iDstPort);
		SEND_DATA sendData = { ipHeader , tcpHeader };

		addrRecver.sin_port = htons(iDstPort);//set receiver prot

		iResult = sendto(pWnd->m_sockRawSend, (const char *)&sendData, sizeof(SEND_DATA), 0, (sockaddr *)&addrRecver, sizeof(sockaddr));
		if (iResult == SOCKET_ERROR) {
			bSended = false;
		}
		//Set IP
		pWnd->m_clistctrlScanResults.InsertItem(
			pWnd->m_clistctrlScanResults.GetItemCount(),
			cstrDstIP
		);
		//Set Port
		cstrPortTemp.Format(TEXT("%u"), iDstPort);
		pWnd->m_clistctrlScanResults.SetItemText(
			pWnd->m_clistctrlScanResults.GetItemCount() - 1,
			1,
			cstrPortTemp
		);
		//Set Send status
		pWnd->m_clistctrlScanResults.SetItemText(
			pWnd->m_clistctrlScanResults.GetItemCount() - 1,
			2,
			bSended ?  TEXT("Success") : TEXT("Failed")
		);
	}
	//Set Timer to cope with the recv time out event
	CString cstrTimeOut;
	pWnd->m_editRecvTimeOut.GetWindowTextW(cstrTimeOut);
	pWnd->SetTimer(TIMER_ID_RECV_TIMEOUT, atoi(CStringA(cstrTimeOut)), NULL);
	return 0;
}

DWORD WINAPI ThreadProcRecv(
	_In_ LPVOID lpParameter
) {
	CSYNScannerDlg * pWnd = (CSYNScannerDlg *)lpParameter;

	//Use Windows Title to show status
	CString cstrWindowsTitle = pWnd->m_cstrWindowsText;
	cstrWindowsTitle.Append(TEXT("(Receiving Results..)"));
	pWnd->SetWindowTextW(cstrWindowsTitle);

	//Judge weather the src ip equal local ip
	DWORD dwSrcIP = 0;
	CStringA cstraMyIP(pWnd->m_cstrMyIP);
	pWnd->m_ipaddrSrc.GetAddress(dwSrcIP);
	if (inet_addr(cstraMyIP) != ntohl(dwSrcIP)) {
		CString str;
		str.Format(TEXT("The source IP address is inconsistent with the local IP address and cannot obtain the scan result"));
		pWnd->MessageBox(str, TEXT("WORNING"), MB_ICONWARNING);
		return 0;
	}

	int iResult = 0;
	SOCKET sockRawRecv = INVALID_SOCKET;
	//Create Raw Socket to receive.
	iResult = sockRawRecv = pWnd->m_sockRawRecv = socket(AF_INET, SOCK_RAW, IPPROTO_IP);
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("ThreadProcRecv socket failed with error : %d"), WSAGetLastError());
		pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		return -1;
	}

	//set IP_HDRINCL to socket for modify raw ip data by yourself
	DWORD  bRaw = TRUE;
	iResult = setsockopt(sockRawRecv, IPPROTO_IP, IP_HDRINCL, (const char *)&bRaw, sizeof(bRaw));
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("ThreadProcRecv setsockopt IP_HDRINCL failed with error : %d"), WSAGetLastError());
		pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		closesocket(sockRawRecv);
		return -1;
	}

	//bind local port
	sockaddr_in sockAddr = { 0 };
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(0);
	sockAddr.sin_addr.S_un.S_addr = inet_addr(cstraMyIP);
	iResult = bind(sockRawRecv, (const sockaddr *)&sockAddr, sizeof(sockaddr));
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("ThreadProcRecv bind failed with error : %d"), WSAGetLastError());
		pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		closesocket(sockRawRecv);
		return -1;
	}

	//set SIO_RCVALL to sock_raw for receive all ip packets.
	DWORD uiOptval = RCVALL_ON;
	DWORD dwBytesRet = 0;
	iResult = WSAIoctl(sockRawRecv,
		SIO_RCVALL,
		&uiOptval,
		sizeof(uiOptval),
		NULL,
		0,
		&dwBytesRet,
		NULL,
		NULL);
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("ThreadProcRecv WSAIoctl SIO_RCVALL failed with error : %d"), WSAGetLastError());
		pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		closesocket(sockRawRecv);
		return -1;
	}

	//recevie and filter packets.
	sockaddr_in sockAddrSrc = { 0 };
	int iLenSockAddr = sizeof(sockaddr_in);

	char * recvbuf = NULL;
	//recv buf
	recvbuf = (char *)malloc(sizeof(char) * RECV_BUF_SIZE);

	//Create filter's data
	DWORD uinSrcIP = 0;//SRC IP
	pWnd->m_ipaddrDst.GetAddress(uinSrcIP);
	uinSrcIP = htonl(uinSrcIP);
	//SRC PORT RANGE
	USHORT ushSrcStartPort = 0,//SRC Start port
		ushSrcEndPort = 0;//SRC End port
	CString cstrwTemp;
	CStringA cstraTemp;
	pWnd->m_editDstStartPort.GetWindowTextW(cstrwTemp);
	cstraTemp = cstrwTemp;
	ushSrcStartPort = LOWORD(atoi(cstraTemp));
	pWnd->m_editDstEndPort.GetWindowTextW(cstrwTemp);
	cstraTemp = cstrwTemp;
	ushSrcEndPort = LOWORD(atoi(cstraTemp));

	//open port list
	char * cPortsStatus = (char *)malloc(0xffff);
	memset(cPortsStatus, 0, 0xffff);

	//Create Send Thread
	pWnd->m_hThreadSend = CreateThread(NULL,
		0,
		ThreadProcSend,
		pWnd,
		0,
		NULL);
	if (pWnd->m_hThreadSend == NULL) {
		CString str;
		str.Format(TEXT("create send thread failed with error : %u"), GetLastError());
		pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		return -1;
	}
	//start recv
	while (true) {
		int iRecvLen = recvfrom(sockRawRecv, recvbuf, sizeof(char) * RECV_BUF_SIZE, 0, (sockaddr *)&sockAddrSrc, &iLenSockAddr);
		if (iRecvLen == 0) {
			break;
		}
		else if (iRecvLen == SOCKET_ERROR) {
			int iErrorCode = WSAGetLastError();
			if (iErrorCode == 10004 || iErrorCode == 10038) {
				//because of closed correctly socket. ( 10004 A blocking operation was interrupted by a call to WSACancelBlockingCall)
				//( 10038 An operation was attempted on something that is not a socket. )
				break;
			}
			CString str;
			str.Format(TEXT("ThreadProcRecv recvfrom failed with error : %d"), iErrorCode);
			pWnd->MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
			closesocket(sockRawRecv);
			break;
		}
		else if (iRecvLen > 0) {
			//Decode IP and TCP Header
			PIP_HEADER pipHeader = (PIP_HEADER)recvbuf;
			PTCP_HEADER ptcpHeader = (PTCP_HEADER)(recvbuf + (pipHeader->ucVerAndLen & 0x0f) * 4);

			
			USHORT ushSrcPort = ntohs(ptcpHeader->usSrcPort);
			//filter by ip and range of port
			if (pipHeader->uiSrcIp == uinSrcIP &&//check ip
				ushSrcPort >= ushSrcStartPort &&
				ushSrcPort <= ushSrcEndPort && 
				!cPortsStatus[ushSrcPort] &&//Eliminate duplicates of port
				ptcpHeader->uiConfirmSerialNum == 0x01000000 &&//check ack number 0x01000000(network)
				((ntohs(ptcpHeader->usHeaderLenAndFlag) & 0x0012) == 0x0012)//Flags have ACK and SYN
				) {

				//save the received valid data to data.txt
				FILE * pfile = NULL;
				pfile = fopen("data.txt", "a+");
				fprintf(pfile, "\n\n接收到的数据长度为 : %d ip :%s prot : %u\n", iRecvLen, inet_ntoa(*(in_addr *)&sockAddrSrc.sin_addr.S_un.S_addr), ntohs(ptcpHeader->usSrcPort));
				fwrite(recvbuf, iRecvLen, 1, pfile);
				fclose(pfile);

				//save port status
				cPortsStatus[ushSrcPort] = 1;

				//add result to m_clistctrlValidPorts
				CString cstrOpenPort;
				cstrOpenPort.Format(TEXT("%u"), ushSrcPort);
				pWnd->m_clistctrlValidPorts.InsertItem(
					pWnd->m_clistctrlValidPorts.GetItemCount(),
					cstrOpenPort
				);
			}
		}
	}
	closesocket(sockRawRecv);
	free(cPortsStatus);
	free(recvbuf);
	cPortsStatus = NULL;
	recvbuf = NULL;
	//Use Windows Title to show status
	cstrWindowsTitle = pWnd->m_cstrWindowsText;
	cstrWindowsTitle.Append(TEXT("(Scan Finished)"));
	pWnd->SetWindowTextW(cstrWindowsTitle);
	return 0;
}

void CSYNScannerDlg::OnClickedButtonScan()
{
	// TODO: Add your control notification handler code here
	//Use Windows Title to show status
	CString cstrWindowText(m_cstrWindowsText);
	cstrWindowText.Append(TEXT("(Scanning...)"));
	SetWindowText(cstrWindowText);

	//empty list
	m_clistctrlScanResults.DeleteAllItems();
	m_clistctrlValidPorts.DeleteAllItems();

	//Valiate Input
	if (m_editDstStartPort.GetWindowTextLengthW() < 1 || 
		m_editDstEndPort.GetWindowTextLengthW() < 1 ||
		m_editSrcPort.GetWindowTextLengthW() < 1 ||
		m_editRecvTimeOut.GetWindowTextLengthW() < 1) 
	{
		MessageBox(TEXT("All parament need filled!"), TEXT("WORNING"), MB_ICONWARNING);
		return;
	}
	if (m_ipaddrDst.IsBlank() ||
		m_ipaddrSrc.IsBlank()) {
		MessageBox(TEXT("All parament need filled!"), TEXT("WORNING"), MB_ICONWARNING);
		return;
	}

	//Start to scan

	//Create Recv Thread (Send thread will create in recv thread after preparation of recv socket creation finished)
	m_hThreadRecv = CreateThread(NULL,
		0,
		ThreadProcRecv,
		this,
		0,
		NULL);
	if (m_hThreadRecv == NULL) {
		CString str;
		str.Format(TEXT("create recv thread failed with error : %u"), GetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
		return;
	}

	m_cbtnScan.EnableWindow(false);
	return;
}


void CSYNScannerDlg::OnClose()
{
	// TODO: Add your message handler code here and/or call default
	closesocket(m_sockRawSend);
	int iResult = 0;
	iResult = WSACleanup();
	if (iResult == SOCKET_ERROR) {
		CString str;
		str.Format(TEXT("WSACleanup failed with error : %d"), WSAGetLastError());
		MessageBox(str, TEXT("ERROR"), MB_ICONERROR);
	}
	CDialogEx::OnClose();
}


void CSYNScannerDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	if (nIDEvent == TIMER_ID_RECV_TIMEOUT) {
		KillTimer(TIMER_ID_RECV_TIMEOUT);
		closesocket(m_sockRawRecv);
		m_cbtnScan.EnableWindow(true);
	}

	CDialogEx::OnTimer(nIDEvent);
}
