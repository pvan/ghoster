
#include <Ole2.h>
#pragma comment(lib, "Ole32.lib")

void (*dd_on_dragdrop)(char *str);  // application-defined callback for drag drop event

void dd_process_data_object(IDataObject *pdto)
{
    // start with trying CF_HDROP (file drop) since CF_TEXT seems to pass GetData even on file drops
    FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;

    HRESULT hr = pdto->GetData(&fmte, &medium);

    if (FAILED(hr))
    {
        fmte.cfFormat = CF_TEXT;
        // OutputDebugString("not a file drop, trying text\n");
        hr = pdto->GetData(&fmte, &medium);
    }

    if (!FAILED(hr))
    {
        HGLOBAL hfiles = medium.hGlobal;
        HDROP hdrop = (HDROP)GlobalLock(hfiles);
        TCHAR str[MAX_PATH];

        if (fmte.cfFormat == CF_TEXT)
        {
            // PRINT("medium.hGlobal %s\n", (char*)hdrop);
            strncpy(str, (char*)hdrop, MAX_PATH);
        }

        if (fmte.cfFormat == CF_HDROP)
        {
            UINT cch = DragQueryFile(hdrop, 0, str, MAX_PATH);  // 0 = just get the first one
        }

        PRINT("\n\nDRAGDROP: %s\n\n", str);

        // or could pass WM_DROPFILES to hwnd maybe
        if (dd_on_dragdrop) dd_on_dragdrop(str);

        GlobalUnlock(hfiles);
        // ReleaseStgMedium(&medium); // todo: add this
    }
    else { OutputDebugString("Cannot handle dropped item\n"); return; }
}


// basically from raymond chen's https://blogs.msdn.microsoft.com/oldnewthing/20100503-00/?p=14183
class dd_DropTarget : public IDropTarget
{
    public:
    dd_DropTarget() { m_cRef = 1; }
    ~dd_DropTarget() {  }

    // *** IUnknown ***
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
            return cRef;
    }

    // *** IDropTarget ***
    STDMETHODIMP DragEnter(IDataObject *pdto, DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
    {
        *pdwEffect &= DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
    {
        *pdwEffect &= DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragLeave()
    {
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject *pdto, DWORD grfKeyState, POINTL ptl, DWORD *pdwEffect)
    {
        dd_process_data_object(pdto);
        *pdwEffect &= DROPEFFECT_COPY;
        return S_OK;
    }

    private:
    LONG m_cRef;
};



void dd_init(HWND hwnd, void (*dropproc)(char*))
{
    OleInitialize(NULL);
    dd_DropTarget *dropTarget = new dd_DropTarget();
    RegisterDragDrop(hwnd, dropTarget);

    dd_on_dragdrop = dropproc;
}

