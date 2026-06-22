#SingleInstance Force
#NoEnv
SendMode Input
SetWorkingDir %A_ScriptDir%
CoordMode, Mouse, Screen
CoordMode, Pixel, Screen

; ========== Settings ==========
reconnectClickX := 1500
reconnectClickY := 66
loopDelayMs := 8000

; ========== State ==========
running := false
macroBusy := false
targetWindowId := ""

; F8: Start/Stop
F8::
    running := !running
    if (running) {
        WinGet, targetWindowId, ID, A
        Tooltip, 10.5s Macro gestartet: F8 stoppt erneut, F9 stoppt sofort
        SetTimer, HideTooltip, -1000
        SetTimer, RunMacro, -1
    } else {
        SetTimer, RunMacro, Off
        Tooltip, Macro gestoppt
        SetTimer, HideTooltip, -1000
    }
return

; F9: Sofort stoppen
F9::
    running := false
    macroBusy := false
    SetTimer, RunMacro, Off
    targetWindowId := ""
    Tooltip, Macro sofort gestoppt
    SetTimer, HideTooltip, -1000
return

RunMacro:
    if (!running || macroBusy) {
        return
    }
    macroBusy := true
    ExecuteCycle()
    macroBusy := false
    if (running) {
        SetTimer, RunMacro, % "-" . loopDelayMs
    }
return

ExecuteCycle() {
    global targetWindowId
    global reconnectClickX, reconnectClickY

    if (targetWindowId) {
        WinActivate, ahk_id %targetWindowId%
        Sleep 60
    }

    SendInput, o
    Sleep 500
    DirectClickReconnect()
}

DirectClickReconnect() {
    global targetWindowId, reconnectClickX, reconnectClickY

    if (targetWindowId) {
        WinActivate, ahk_id %targetWindowId%
        Sleep 80
    }

    MouseMove, %reconnectClickX%, %reconnectClickY%, 0
    Sleep 40
    Loop, 3 {
        Click, %reconnectClickX%, %reconnectClickY%
        Sleep 250
    }
}

HideTooltip:
    Tooltip
return
