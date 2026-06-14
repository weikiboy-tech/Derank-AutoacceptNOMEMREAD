#SingleInstance Force
#NoEnv
SendMode Input
SetWorkingDir %A_ScriptDir%
CoordMode, Mouse, Screen
CoordMode, Pixel, Screen

; ========== Settings ==========
consoleOpenKey := "{SC029}"
disconnectCommand := "disconnect"

reconnectSearchLeft := 0
reconnectSearchTop := 0
reconnectSearchRight := A_ScreenWidth
reconnectSearchBottom := 220
reconnectClickX := 1500
reconnectClickY := 66

reconnectImage := A_ScriptDir . "\reconnect.png"
reconnectImageWidth := 104
reconnectImageHeight := 51
reconnectImageVariation := 70
useImageSearch := true

greenColor := 0x29A329
greenVariation := 32

; Alte Methode: nach Reconnect-Klick einfach 7,9 Sekunden warten.
loopDelayMs := 7900

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
    global consoleOpenKey, disconnectCommand, targetWindowId
    global reconnectClickX, reconnectClickY

    ToggleConsole()
    Sleep 180

    if (targetWindowId) {
        WinActivate, ahk_id %targetWindowId%
        Sleep 60
    }

    SendRaw, %disconnectCommand%
    Sleep 60
    SendInput, {Enter}

    Sleep 40
    ToggleConsole()

    Sleep 500
    DirectClickReconnect()
}

ToggleConsole() {
    global consoleOpenKey
    SendInput, %consoleOpenKey%
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

WaitAndClickReconnect() {
    global running, reconnectSearchLeft, reconnectSearchTop, reconnectSearchRight, reconnectSearchBottom
    global useImageSearch, reconnectImage, reconnectImageWidth, reconnectImageHeight, reconnectImageVariation
    global greenColor, greenVariation

    Loop {
        if (!running) {
            return false
        }

        if (useImageSearch && FileExist(reconnectImage)) {
            ImageSearch, bx, by, %reconnectSearchLeft%, %reconnectSearchTop%, %reconnectSearchRight%, %reconnectSearchBottom%, *%reconnectImageVariation% %reconnectImage%
            if (ErrorLevel = 0) {
                clickX := bx + (reconnectImageWidth // 2)
                clickY := by + (reconnectImageHeight // 2)
                Click, %clickX%, %clickY%
                Sleep 120
                return true
            }
        }

        PixelSearch, px, py, %reconnectSearchLeft%, %reconnectSearchTop%, %reconnectSearchRight%, %reconnectSearchBottom%, %greenColor%, %greenVariation%, Fast RGB
        if (ErrorLevel = 0) {
            Click, %px%, %py%
            Sleep 120
            return true
        }

        Sleep 100
    }
}

HideTooltip:
    Tooltip
return
