#include "Manager.h"
#include "FileScan.h"
SCANTASK_LINKEDLIST ScanTask; //扫描管理链表
HANDLE ThreadScanTicket;
HANDLE ThreadPool_Scan[SCAN_POOL_MAX];
HANDLE ThreadPool_Scan_MainEvent;
HANDLE ThreadPool_Scan_ThreadEvent;
HANDLE ThreadPool_Scan_QueueEvent;
ENGINE_INFORMATION Engines[ENGINES_NUM] ;
void OnVirus(PVOID TaskHandle,PVIRUS_DESCRIPTION PVir,VirusType Type)
{

}
void OnFinish(PVOID TaskHandle,int ExitCode)
{

}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
	
	InitializeScanManager();
		CreateScanTask(ScanTaskType::QuickIndentify,L"D:\\virus样本2.exe",0,(PF_SCAN_FINISH_CALLBACK)OnFinish,0,0,0);

	//CreateScanTask(ScanTaskType::SelectScan,L"E:\\工具大全",OnVirus,(PF_SCAN_FINISH_CALLBACK),0);
	//CreateScanTask(ScanTaskType::QuickIndentify,L"C:\\1.exe",0,0,0);
	while(1)Sleep(100);
	exit(0);
	
}

PSCANTASK_LINKEDLIST InsertScanTask(ScanTaskType Type,HANDLE ThreadHandle)
{
	PSCANTASK_LINKEDLIST CurrentTask = &ScanTask;
	int ID;
	while(TRUE)
	{
	ID = CurrentTask->ID;
	if(CurrentTask->NextTask == NULL)
	{

		CurrentTask->NextTask = (PSCANTASK_LINKEDLIST)malloc(sizeof(SCANTASK_LINKEDLIST));
		CurrentTask = (PSCANTASK_LINKEDLIST)CurrentTask->NextTask;
		ZeroMemory(CurrentTask,sizeof(SCANTASK_LINKEDLIST));
		CurrentTask->Type = Type;
		CurrentTask->ID = ID + 1;
		CurrentTask->ThreadHandle = ThreadHandle;
		return CurrentTask;
	}
	CurrentTask = (PSCANTASK_LINKEDLIST)CurrentTask->NextTask;
	}
	return 0;
}
void RemoveScanTask(int TaskID)
{
	
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)&ScanTask;
	PSCANTASK_LINKEDLIST PreviousTask;
	while(Task != NULL)
	{
	if(Task->ID == TaskID)
	{
		PreviousTask->NextTask = Task->NextTask;
		free(Task);
		break;
	}
	PreviousTask = Task;
	Task = (PSCANTASK_LINKEDLIST)Task->NextTask;
	}
}
void ClearScanTask()
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)&ScanTask;
	PSCANTASK_LINKEDLIST PreviousTask;
	while(Task != NULL)
	{
	PreviousTask = Task;
	Task = (PSCANTASK_LINKEDLIST)Task->NextTask;
	if(PreviousTask->ID != 0)
	{
		TerminateThread(PreviousTask->ThreadHandle,0);
		free(PreviousTask);
	}
	}
}
PVOID CreateScanTask(ScanTaskType Type,wchar_t* Path,PF_SCAN_VIRUS_CALLBACK VirusCallBack,PF_SCAN_FINISH_CALLBACK FinishCallBack,PF_SCAN_ERROR_CALLBACK ErrorCallBack,DWORD32 CallBackPtr32,DWORD32 CallBackValue)
{
	PSCAN_INFORMATION Info;
	if(Type != QuickIndentify)
	{
	Info = (PSCAN_INFORMATION)malloc(sizeof(SCAN_INFORMATION));
	Info->FinishCallBack = FinishCallBack;
	Info->VirusCallBack = VirusCallBack;
	Info->ErrorCallBack = ErrorCallBack;
	Info->CallBackPtr32 = CallBackPtr32;
	Info->CallBackValue = CallBackValue;
	Info->TaskHandle = 0;
	}
	switch (Type)
	{
	case QuickScan:
		GetWindowsDirectory(Info->Path,MAX_PATH);
		Info->TaskHandle = InsertScanTask(Type,StartThread(NormalScanThread,Info));
		break;
	case FullScan:
		Info->TaskHandle = InsertScanTask(Type,StartThread(FullScanThread,Info));
		break;
	case SelectScan:
		lstrcpy(Info->Path,Path);
		Info->TaskHandle = InsertScanTask(Type,StartThread(NormalScanThread,Info));
		break;
	case QuickIndentify:
		if(IndentifyTaskCount == SCAN_POOL_MAX)
		{
			PSCAN_QUEUE_INFORMATION QInfo =  (PSCAN_QUEUE_INFORMATION)malloc(sizeof(SCAN_QUEUE_INFORMATION));
			QInfo->Next = IndentifyQueue;
			lstrcpy(QInfo->Info.Path,Path);
			QInfo->Info.FinishCallBack = FinishCallBack;
			QInfo->Info.VirusCallBack = VirusCallBack;
			QInfo->Info.ErrorCallBack = ErrorCallBack;
			QInfo->Info.CallBackPtr32 = CallBackPtr32;
			QInfo->Info.CallBackValue = CallBackValue;
			QInfo->Info.TaskHandle = 0;
			QInfo->Next = NULL;
			WaitForSingleObject(ThreadPool_Scan_QueueEvent,INFINITE);
			if(IndentifyQueue == NULL)
			{
				IndentifyQueueTop = QInfo;
				IndentifyQueue = QInfo;
			}
			else
			{
			IndentifyQueueTop->Next = QInfo;
			IndentifyQueueTop = QInfo;
			}
			SetEvent(ThreadPool_Scan_QueueEvent);
		}
		else
		{
		WaitForSingleObject(ThreadPool_Scan_MainEvent,INFINITE);
		IndentifyTaskCount ++;
		lstrcpy(IndentifyTemp.Path,Path);
		IndentifyTemp.FinishCallBack = FinishCallBack;
		IndentifyTemp.VirusCallBack = VirusCallBack;
		IndentifyTemp.ErrorCallBack = ErrorCallBack;
		IndentifyTemp.TaskHandle = 0;
		IndentifyTemp.CallBackPtr32 = CallBackPtr32;
		IndentifyTemp.CallBackValue = CallBackValue;
		SetEvent(ThreadPool_Scan_ThreadEvent);
		}
		break;
	default:
		break;
	}
	return Info->TaskHandle;
}
DWORD WINAPI ScanTicketThread(PVOID arg)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)&ScanTask;
	PSCANTASK_LINKEDLIST PreviousTask;
	DWORD Times;
loop:
	Times = GetTickCount();
	while(1)
	{
	Sleep(1);
	if(GetTickCount()-Times > 1000)
	break;
	}
	Task = (PSCANTASK_LINKEDLIST)&ScanTask;
	while(Task != NULL)
	{
	if(Task->Status.ScanStatus != ScanTaskStatus::Suspended)
	{
		
		Task->Status.TimeUsed ++;
	}
	PreviousTask = Task;
	Task = (PSCANTASK_LINKEDLIST)Task->NextTask;
	}
	goto loop;
	return 0;
}
DWORD WINAPI FullScanThread(PVOID arg)
{
	int DriverType;
	SCAN_INFORMATION Info;
	PSCAN_INFORMATION InfoOld = (PSCAN_INFORMATION)arg;
	while(InfoOld->TaskHandle == 0)Sleep(10); //等待插入到链表完毕
	memcpy(&Info,InfoOld,sizeof(SCAN_INFORMATION));
	free(arg);
	for (wchar_t i=65;i<91;i++)
	{
	Info.Path[0] = i;
	Info.Path[1] = ':';
	Info.Path[2] = '\\';
	Info.Path[3] = '\0';
	DriverType = GetDriveType(Info.Path);
	if (DriverType !=0 && DriverType != 1)
	{
		FileTraversal(&Info);
	}
	}
	Info.FinishCallBack(Info.TaskHandle,Automatic);
	RemoveScanTask(Info.TaskHandle->ID);
	return 0;
}
DWORD WINAPI NormalScanThread(PVOID arg)
{
	SCAN_INFORMATION Info;
	PSCAN_INFORMATION InfoOld = (PSCAN_INFORMATION)arg;
	while(InfoOld->TaskHandle == 0)Sleep(10); //等待插入到链表完毕
	memcpy(&Info,InfoOld,sizeof(SCAN_INFORMATION));
	free(arg);
	FileTraversal(&Info);
	Info.FinishCallBack(Info.TaskHandle,Automatic);
	RemoveScanTask(Info.TaskHandle->ID);
	return 0;
}
void LoadDllForEngine(const wchar_t* DllPath,const char* FunctionName,PENGINE_INFORMATION EInfo)
{
	EInfo->hDll = LoadLibrary(DllPath);
	EInfo->FunctionCall = (SCANENGINE_NORMALCALL)GetProcAddress(EInfo->hDll,FunctionName);
	EInfo->Init = (SCANENGINE_INITCALL)GetProcAddress(EInfo->hDll,"InitlizeEngine");
	EInfo->Term = (SCANENGINE_INITCALL)GetProcAddress(EInfo->hDll,"TerminateEngine");
	if(EInfo->Init != NULL) EInfo->Init();
}
//=====================Export==========================//
void InitializeScanManager()
{
	ZeroMemory(&ScanTask,sizeof(ScanTask));
	ThreadScanTicket = StartThread(ScanTicketThread,0);
	ThreadPool_Scan_MainEvent = CreateEvent(NULL,FALSE,TRUE,NULL); //同步事件
	ThreadPool_Scan_ThreadEvent = CreateEvent(NULL,FALSE,FALSE,NULL); //同步事件
	ThreadPool_Scan_QueueEvent = CreateEvent(NULL,FALSE,TRUE,NULL); //同步事件
	IndentifyQueueTop = IndentifyQueue;
	for (int i = 0; i < SCAN_POOL_MAX; i++)
	{
		ThreadPool_Scan[i] = StartThread(ScanThreadPool,0);
	}
	for (int i = 0; i < ENGINES_NUM; i++)
	{
		switch (i)
		{
		case ScanEngine::HeurDet:
			LoadDllForEngine(L"AsHeurDet.dll","HeurScan",&Engines[i]);
			break;
		case ScanEngine::FileSDVer:
			LoadDllForEngine(L"AsFileSDVer.dll","VerifyFileDigitalSignature",&Engines[i]);
			break;
		}
	}
	
}
void TerminateScanManager()
{
	PSCAN_QUEUE_INFORMATION Queue;
	TerminateThread(ThreadScanTicket,0);
	for (int i = 0; i < SCAN_POOL_MAX; i++)
	{
		TerminateThread(ThreadPool_Scan[i],0);
	}
	for (int i = 0; i < ENGINES_NUM; i++)
	{
		if(Engines[i].Term != NULL) Engines[i].Term();
		FreeLibrary(Engines[i].hDll);
	}
	
	while(IndentifyQueue != NULL)
	{
		Queue = IndentifyQueue;
		IndentifyQueue = (PSCAN_QUEUE_INFORMATION)IndentifyQueue->Next;
		free(Queue);
	}
	CloseHandle(ThreadPool_Scan_MainEvent);
	CloseHandle(ThreadPool_Scan_ThreadEvent);
	CloseHandle(ThreadPool_Scan_QueueEvent);
}
int GetScanTaskScanedCount(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.ScanedCount;
}
int GetScanTaskVirusCount(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.VirusCount;
}
int GetScanTaskTicketCount(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.TimeUsed;
}
void SetScanTaskScanStatusToNormal(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	Task->Status.ScanStatus = ScanTaskStatus::Normal;
}
void SetScanTaskScanStatusToStopped(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	Task->Status.ScanStatus = ScanTaskStatus::Stoped;
}
void SetScanTaskScanStatusToSuspended(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	Task->Status.ScanStatus = ScanTaskStatus::Suspended;
}
bool IsScanTaskScanStatusStopped(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.ScanStatus == ScanTaskStatus::Stoped;
}
bool IsScanTaskScanStatusSuspended(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.ScanStatus == ScanTaskStatus::Suspended;
}
bool IsScanTaskScanStatusNormal(PVOID TaskHandle)
{
	PSCANTASK_LINKEDLIST Task = (PSCANTASK_LINKEDLIST)TaskHandle;
	return Task->Status.ScanStatus == ScanTaskStatus::Normal;
}