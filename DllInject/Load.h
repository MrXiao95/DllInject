#include <Windows.h>
#include <Winternl.h>
#ifdef _WIN64
typedef  DWORD64 DWORDX;
#else
typedef  DWORD32 DWORDX;
#endif
typedef NTSTATUS(WINAPI *LdrGetProcedureAddressT)(IN PVOID DllHandle, IN PANSI_STRING ProcedureName OPTIONAL, IN ULONG ProcedureNumber OPTIONAL, OUT FARPROC *ProcedureAddress);
typedef VOID(WINAPI *RtlFreeUnicodeStringT)(_Inout_ PUNICODE_STRING UnicodeString);
typedef  VOID(WINAPI *RtlInitAnsiStringT)(_Out_    PANSI_STRING DestinationString, _In_opt_ PCSZ         SourceString);
typedef NTSTATUS(WINAPI *RtlAnsiStringToUnicodeStringT)(_Inout_ PUNICODE_STRING DestinationString, _In_ PCANSI_STRING SourceString, _In_ BOOLEAN AllocateDestinationString);
typedef NTSTATUS(WINAPI *LdrLoadDllT)(PWCHAR, PULONG, PUNICODE_STRING, PHANDLE);
typedef BOOL(APIENTRY *ProcDllMain)(LPVOID, DWORD, LPVOID);
typedef NTSTATUS(WINAPI *NtAllocateVirtualMemoryT)(IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN ULONG ZeroBits, IN OUT PSIZE_T RegionSize, IN ULONG AllocationType, IN ULONG Protect);

struct PARAMX
{
	PVOID lpFileData;
	DWORD DataLength;
	LdrGetProcedureAddressT LdrGetProcedureAddress;
	NtAllocateVirtualMemoryT dwNtAllocateVirtualMemory;
	LdrLoadDllT pLdrLoadDll;
	RtlInitAnsiStringT RtlInitAnsiString;
	RtlAnsiStringToUnicodeStringT RtlAnsiStringToUnicodeString;
	RtlFreeUnicodeStringT RtlFreeUnicodeString;


};

DWORDX WINAPI MemLoadLibrary2(PARAMX *X)//2502
{

	LPCVOID lpFileData = X->lpFileData;
	DWORDX DataLength = X->DataLength;

	/****************��ʼ�����ú���********************/
	LdrGetProcedureAddressT LdrGetProcedureAddress = (X->LdrGetProcedureAddress);

	NtAllocateVirtualMemoryT pNtAllocateVirtualMemory = (X->dwNtAllocateVirtualMemory);
	LdrLoadDllT pLdrLoadDll = (X->pLdrLoadDll);
	RtlInitAnsiStringT RtlInitAnsiString = X->RtlInitAnsiString;
	RtlAnsiStringToUnicodeStringT RtlAnsiStringToUnicodeString = X->RtlAnsiStringToUnicodeString;
	RtlFreeUnicodeStringT RtlFreeUnicodeString = X->RtlFreeUnicodeString;

	ProcDllMain pDllMain = NULL;
	void *pMemoryAddress = NULL;



	ANSI_STRING ansiStr;
	UNICODE_STRING UnicodeString;
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pNTHeader;
	PIMAGE_SECTION_HEADER pSectionHeader;
	int ImageSize = 0;

	int nAlign = 0;
	int i = 0;


	//���������Ч�ԣ�����ʼ��
	/*********************CheckDataValide**************************************/
	//	PIMAGE_DOS_HEADER pDosHeader;
	//��鳤��
	if (DataLength > sizeof(IMAGE_DOS_HEADER))
	{
		pDosHeader = (PIMAGE_DOS_HEADER)lpFileData; // DOSͷ
													//���dosͷ�ı��
		if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) goto CODEEXIT; //0��5A4D : MZ

																	   //��鳤��
		if ((DWORDX)DataLength < (pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS))) goto CODEEXIT;
		//ȡ��peͷ
		pNTHeader = (PIMAGE_NT_HEADERS)((DWORDX)lpFileData + pDosHeader->e_lfanew); // PEͷ
																					//���peͷ�ĺϷ���
		if (pNTHeader->Signature != IMAGE_NT_SIGNATURE) goto CODEEXIT; //0��00004550: PE00
		if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_DLL) == 0) //0��2000: File is a DLL
			goto CODEEXIT;
		if ((pNTHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) //0��0002: ָ���ļ���������
			goto CODEEXIT;
		if (pNTHeader->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))
			goto CODEEXIT;


		//ȡ�ýڱ��α�
		pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORDX)pNTHeader + sizeof(IMAGE_NT_HEADERS));
		//��֤ÿ���ڱ�Ŀռ�
		for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
		{
			if ((pSectionHeader[i].PointerToRawData + pSectionHeader[i].SizeOfRawData) > (DWORD)DataLength) goto CODEEXIT;
		}

		/**********************************************************************/
		nAlign = pNTHeader->OptionalHeader.SectionAlignment; //�ζ����ֽ���

															 //ImageSize = pNTHeader->OptionalHeader.SizeOfImage;
															 //// ��������ͷ�ĳߴ硣����dos, coff, peͷ �� �α�Ĵ�С
		ImageSize = (pNTHeader->OptionalHeader.SizeOfHeaders + nAlign - 1) / nAlign * nAlign;
		// �������нڵĴ�С
		for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
		{
			//�õ��ýڵĴ�С
			int CodeSize = pSectionHeader[i].Misc.VirtualSize;
			int LoadSize = pSectionHeader[i].SizeOfRawData;
			int MaxSize = (LoadSize > CodeSize) ? (LoadSize) : (CodeSize);

			int SectionSize = (pSectionHeader[i].VirtualAddress + MaxSize + nAlign - 1) / nAlign * nAlign;
			if (ImageSize < SectionSize)
				ImageSize = SectionSize; //Use the Max;
		}
		if (ImageSize == 0) goto CODEEXIT;

		// ���������ڴ�
		SIZE_T uSize = ImageSize;
		pNtAllocateVirtualMemory((HANDLE)-1, &pMemoryAddress, 0, &uSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		if (pMemoryAddress != NULL)
		{

			// ������Ҫ���Ƶ�PEͷ+�α��ֽ���
			int HeaderSize = pNTHeader->OptionalHeader.SizeOfHeaders;
			int SectionSize = pNTHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
			int MoveSize = HeaderSize + SectionSize;
			//����ͷ�Ͷ���Ϣ
			for (i = 0; i < MoveSize; i++)
			{
				*((PCHAR)pMemoryAddress + i) = *((PCHAR)lpFileData + i);
			}
			//memmove(pMemoryAddress, lpFileData, MoveSize);//Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����

			//����ÿ����
			for (i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i)
			{
				if (pSectionHeader[i].VirtualAddress == 0 || pSectionHeader[i].SizeOfRawData == 0)continue;
				// ��λ�ý����ڴ��е�λ��
				void *pSectionAddress = (void *)((DWORDX)pMemoryAddress + pSectionHeader[i].VirtualAddress);
				// ���ƶ����ݵ������ڴ�
				//	memmove((void *)pSectionAddress,(void *)((DWORDX)lpFileData + pSectionHeader[i].PointerToRawData),	pSectionHeader[i].SizeOfRawData);
				//Ϊ������һ��API,ֱ��������ĵ��ֽڸ������ݾ�����
				for (size_t k = 0; k < pSectionHeader[i].SizeOfRawData; k++)
				{
					*((PCHAR)pSectionAddress + k) = *((PCHAR)lpFileData + pSectionHeader[i].PointerToRawData + k);
				}
			}
			/*******************�ض�λ��Ϣ****************************************************/

			if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress > 0
				&& pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0)
			{

				DWORDX Delta = (DWORDX)pMemoryAddress - pNTHeader->OptionalHeader.ImageBase;
				DWORDX * pAddress;
				//ע���ض�λ���λ�ÿ��ܺ�Ӳ���ļ��е�ƫ�Ƶ�ַ��ͬ��Ӧ��ʹ�ü��غ�ĵ�ַ
				PIMAGE_BASE_RELOCATION pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pMemoryAddress
					+ pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
				while ((pLoc->VirtualAddress + pLoc->SizeOfBlock) != 0) //��ʼɨ���ض�λ��
				{
					WORD *pLocData = (WORD *)((DWORDX)pLoc + sizeof(IMAGE_BASE_RELOCATION));
					//���㱾����Ҫ�������ض�λ���ַ������Ŀ
					int NumberOfReloc = (pLoc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
					for (i = 0; i < NumberOfReloc; i++)
					{
						if ((DWORDX)(pLocData[i] & 0xF000) == 0x00003000 || (DWORDX)(pLocData[i] & 0xF000) == 0x0000A000) //����һ����Ҫ�����ĵ�ַ
						{
							// ������
							// pLoc->VirtualAddress = 0��1000;
							// pLocData[i] = 0��313E; ��ʾ����ƫ�Ƶ�ַ0��13E����Ҫ����
							// ��� pAddress = ����ַ + 0��113E
							// ����������� A1 ( 0c d4 02 10) �������ǣ� mov eax , [1002d40c]
							// ��Ҫ����1002d40c�����ַ
							pAddress = (DWORDX *)((DWORDX)pMemoryAddress + pLoc->VirtualAddress + (pLocData[i] & 0x0FFF));
							*pAddress += Delta;
						}
					}
					//ת�Ƶ���һ���ڽ��д���
					pLoc = (PIMAGE_BASE_RELOCATION)((DWORDX)pLoc + pLoc->SizeOfBlock);
				}
				/***********************************************************************/
			}

			/******************* ���������ַ��**************/
			DWORDX Offset = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
			if (Offset == 0)
				goto CODEEXIT; //No Import Table

			PIMAGE_IMPORT_DESCRIPTOR pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pMemoryAddress + Offset);

			PIMAGE_IMPORT_BY_NAME pByName = NULL;
			while (pID->Characteristics != 0)
			{
				PIMAGE_THUNK_DATA pRealIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->FirstThunk);
				PIMAGE_THUNK_DATA pOriginalIAT = (PIMAGE_THUNK_DATA)((DWORDX)pMemoryAddress + pID->OriginalFirstThunk);
				//��ȡdll������
				char* pName = (char*)((DWORDX)pMemoryAddress + pID->Name);
				HANDLE hDll = 0;

				RtlInitAnsiString(&ansiStr, pName);

				RtlAnsiStringToUnicodeString(&UnicodeString, &ansiStr, true);
				pLdrLoadDll(NULL, NULL, &UnicodeString, &hDll);
				RtlFreeUnicodeString(&UnicodeString);

				if (hDll == NULL) {

					goto CODEEXIT; //NOT FOUND DLL
				}

				//��ȡDLL��ÿ�����������ĵ�ַ������IAT
				//ÿ��IAT�ṹ�� ��
				// union { PBYTE ForwarderString;
				// PDWORDX Function;
				// DWORDX Ordinal;
				// PIMAGE_IMPORT_BY_NAME AddressOfData;
				// } u1;
				// ������һ��DWORDX ����������һ����ַ��
				for (i = 0; ; i++)
				{
					if (pOriginalIAT[i].u1.Function == 0)break;
					FARPROC lpFunction = NULL;
					if (IMAGE_SNAP_BY_ORDINAL(pOriginalIAT[i].u1.Ordinal)) //�����ֵ�������ǵ������
					{
						if (IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal))
						{

							LdrGetProcedureAddress(hDll, NULL, IMAGE_ORDINAL(pOriginalIAT[i].u1.Ordinal), &lpFunction);
						}
					}
					else//�������ֵ���
					{
						//��ȡ��IAT���������ĺ�������
						pByName = (PIMAGE_IMPORT_BY_NAME)((DWORDX)pMemoryAddress + (DWORDX)(pOriginalIAT[i].u1.AddressOfData));
						if ((char *)pByName->Name)
						{
							RtlInitAnsiString(&ansiStr, (char *)pByName->Name);
							LdrGetProcedureAddress(hDll, &ansiStr, 0, &lpFunction);

						}

					}

					//���***********

					if (lpFunction != NULL) //�ҵ��ˣ�
						pRealIAT[i].u1.Function = (DWORDX)lpFunction;
					else
						goto CODEEXIT;
				}

				//move to next
				pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORDX)pID + sizeof(IMAGE_IMPORT_DESCRIPTOR));
			}

			/***********************************************************/
			//��������ַ
			pNTHeader->OptionalHeader.ImageBase = (DWORDX)pMemoryAddress;

			//NtProtectVirtualMemory((HANDLE)-1, &pMemoryAddress, (PSIZE_T)&ImageSize, PAGE_EXECUTE_READ, &oldProtect);
			pDllMain = (ProcDllMain)(pNTHeader->OptionalHeader.AddressOfEntryPoint + (DWORDX)pMemoryAddress);

			pDllMain(0, DLL_PROCESS_ATTACH, pMemoryAddress);//����Ĳ���1����Ӧ�ô�����(HMODULE)pMemoryAddress,����û��Ҫ,��Ϊ�޷�ʹ����Դ,����û��Ҫ,Ҫʹ����Դ,��̳��������˵�����ʹ��

		}
	}

CODEEXIT:

	return (DWORDX)pMemoryAddress;
}
