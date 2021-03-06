#include "GfxDrvDXGIModeEnumerator.h"
#include "GfxDrvDXGIErrorLogger.h"
#include "DEFS.H"
#include "FELLOW.H"

void GfxDrvDXGIModeEnumerator::EnumerateModes(IDXGIOutput *output, GfxDrvDXGIModeList& modes)
{
  DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
  UINT flags = 0;
  UINT numModes = 0;
  
  HRESULT hr = output->GetDisplayModeList(format, flags, &numModes, NULL);

  if (FAILED(hr))
  {
    GfxDrvDXGIErrorLogger::LogError("GfxDrvDXGIModeEnumerator::EnumerateModes(): Failed to get display mode list.", hr);
    return;
  }

  fellowAddLog("Output has %d modes.\n", numModes);

  DXGI_MODE_DESC *descs = new DXGI_MODE_DESC[numModes];
  hr = output->GetDisplayModeList(format, flags, &numModes, descs);

  if (FAILED(hr))
  {
    GfxDrvDXGIErrorLogger::LogError("GfxDrvDXGIModeEnumerator::EnumerateModes(): Failed to get display mode list.", hr);
    return;
  }

  for (UINT i = 0; i < numModes; i++)
  {
    modes.push_back(new GfxDrvDXGIMode(&descs[i]));
  }
  delete[] descs;
}

void GfxDrvDXGIModeEnumerator::DeleteModeList(GfxDrvDXGIModeList& modes)
{
  for (GfxDrvDXGIModeList::iterator i = modes.begin(); i != modes.end(); ++i)
  {
    delete *i;
  }
}
