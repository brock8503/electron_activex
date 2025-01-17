# Experiment
Get Electron running as an ActiveX control 

## Test tool
Download tstcon32 to test ActiveX http://www.microsoft.com/en-us/download/confirmation.aspx?id=16351

## Build setup
To build it you'll need http://www.microsoft.com/en-us/download/confirmation.aspx?id=40770 installed in vs2013
Update WINVER to #define WINVER 0x0501		// Change this to 0x0500 to target Windows 98 and Windows 2000.
Then project properties -> configurations properties -> C/C++ -> treats warning as error -> No (/WX-)

http://blogs.msdn.com/b/rextang/archive/2008/08/04/8830769.aspx
Configuration Properties -> Expand the Linker node -> Manifest File property page -> Enable User Account Control (UAC), UAC Execution Level, and UAC Bypass UI Protection properties.
Set MANIFEST /NO
set /MD flag

OLEView
https://msdn.microsoft.com/en-us/library/d0kh9f4c.aspx
 "C:\Program Files (x86)\Windows Kits\8.0\bin\x86\oleview.exe"
 
## Setup

1. Register the control with the OS
Use Regsvr32 https://technet.microsoft.com/en-us/library/bb490985.aspx
* Register dll
$ regsvr32.exe ActiveX.dl
* UnRegister dll
$ regsvr32.exe /u ActiveX.dll

2. Test control
Verify registration with OLEView
orTest view with tstcon32

## Build
Only *debug* is building currently. Select run

## Clean build
./vendor/depot_tools/ninja -t clean


## Debug
Using win32 `DebugBreak();` during post build you can break into the `DllRegisterServer`

## What I know
* `atom_browser_main_parts.cc` is the main delegate for the app. It creates the browser and renderer
   * `PreMainMessageLoopRun` is an important method
* `atom_browser_client` is the browser process ie. electron.exe that was initialy launched
* `atom_renderer_client` you guessed it the renderer
* 

## TODO's
* Document the per-user registration including registry keys
* Remove MFC dependency from activeX
* Link against the electron lib (done)
* Set the renderer process to explicitly execute electron_renderer.exe (done)
* Figure out why process is crashing
* Figure out why the observers_ is 0 when it should be 1 after node_bindings is initialized
```
void Browser::DidFinishLaunching() {
  is_ready_ = true;
  // observers isn't getting set
  FOR_EACH_OBSERVER(BrowserObserver, observers_, OnFinishLaunching());
}

```
after node_binding inits App should be instantiated and register. 
```
App::App() {
  Browser::Get()->AddObserver(this);
}
```
Working hypothesis is that node env is not setup correctly. It references the "browser" process which is a little different in our case.

#Reference Material
https://books.google.com/books?id=Me9vBAAAQBAJ&pg=PA28&lpg=PA28&dq=browser-subprocess-path&source=bl&ots=N37hCmjNwA&sig=1sbLm6eBlE9QImLXLVKl4nmsYmI&hl=en&sa=X&ved=0ahUKEwiTkcXYlY3KAhVJ-2MKHX8fAZcQ6AEIUDAH#v=onepage&q=browser-subprocess-path&f=false
https://www.chromium.org/developers/design-documents/aura
https://msdn.microsoft.com/en-us/library/office/ee941475(v=office.14).aspx#BuildingNativeAddinforOL2010_Introduction
https://github.com/nwjs/nw.js/
https://msdn.microsoft.com/en-us/library/windows/desktop/ms678814(v=vs.85).aspx
http://www.codeproject.com/Articles/14533/A-Complete-ActiveX-Web-Control-Tutorial
https://support.microsoft.com/en-us/kb/182598
http://stackoverflow.com/questions/25811091/activex-standard-user-support
https://support.microsoft.com/en-us/kb/161873
http://stackoverflow.com/questions/23218584/loadtypelibex-is-failing-while-running-an-application-in-session-0-service-sess
