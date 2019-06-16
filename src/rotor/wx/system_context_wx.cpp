#include "rotor/wx/system_context_wx.h"
#include "rotor/wx/supervisor_wx.h"

using namespace rotor::wx;

system_context_wx_t::system_context_wx_t(wxAppConsole *app_) { app = app_ ? app_ : wxTheApp; }
