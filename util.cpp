#include "glib.h"
#include <util.h>
#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winnls.h>

HRESULT CreateShortcut(LPCSTR pszTargetfile, LPCSTR pszTargetargs, LPCSTR pszLinkfile, int iShowmode, LPCSTR pszCurdir, LPCSTR pszIconfile, int iIconindex) {
	HRESULT hRes;
	IShellLink *pShellLink;
	IPersistFile *pPersistFile;
	WCHAR wszLinkfile[MAX_PATH];
	
    CoInitialize(NULL);
	hRes = E_INVALIDARG;
	if(
		(pszTargetfile != NULL) && (strlen(pszTargetfile) > 0) &&
		(pszTargetargs != NULL) &&
		(pszLinkfile != NULL) && (strlen(pszLinkfile) > 0) &&
		(iShowmode >= 0) &&
		(pszCurdir != NULL) &&
		(iIconindex >= 0)) {
		hRes = CoCreateInstance(
			CLSID_ShellLink,		/* pre-defined CLSID of the IShellLink object */
			NULL,					/* pointer to parent interface if part of aggregate */
			CLSCTX_INPROC_SERVER,	/* caller and called code are in same process */
			IID_IShellLink,			/* pre-defined interface of the IShellLink object */
			(LPVOID *)&pShellLink); /* Returns a pointer to the IShellLink object */
		if(SUCCEEDED(hRes)) {
			/* Set the fields in the IShellLink object */
			hRes = pShellLink->SetPath(pszTargetfile);
			hRes = pShellLink->SetArguments(pszTargetargs);
			if(iShowmode > 0) {
				hRes = pShellLink->SetShowCmd(iShowmode);
			}
			if(strlen(pszCurdir) > 0) {
				hRes = pShellLink->SetWorkingDirectory(pszCurdir);
			}
			if(pszIconfile && strlen(pszIconfile) > 0 && iIconindex >= 0) {
				hRes = pShellLink->SetIconLocation(pszIconfile, iIconindex);
			}

			/* Use the IPersistFile object to save the shell link */
			hRes = pShellLink->QueryInterface(
				IID_IPersistFile,		  /* pre-defined interface of the
											  IPersistFile object */
				(LPVOID *)&pPersistFile); /* returns a pointer to the
									IPersistFile object */
			if(SUCCEEDED(hRes)) {
				MultiByteToWideChar(CP_ACP, 0,
														pszLinkfile, -1,
														wszLinkfile, MAX_PATH);
				hRes = pPersistFile->Save(wszLinkfile, TRUE);
				pPersistFile->Release();
			}
			pShellLink->Release();
		}
	}
	CoUninitialize();
	return (hRes);
}