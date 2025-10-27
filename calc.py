import math
import pyautogui as pag
import win32con
import win32api
import win32gui as wgui


def test():    print("Calc Test Start")
    # Open Calculator
    pag.press('win')
    pag.write('Calculator')
    pag.press('enter')
    pag.sleep(1)
    pag.write('6')
    pag.press('+')
    pag.write('7')



test()