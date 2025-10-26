import pyautogui
import time


def test():
    x = 37
    y = 21
    c = 37
    while True:
        for i in range(5):
            pyautogui.moveTo(x, y)
            x += 81
            y += 0
            time.sleep(0.1)
        break
    pyautogui.moveTo(36, y+89)
    y = 89
    print(x, y, c)
    for i in range(5):
        c += 81
        print(x, y, c)
        pyautogui.moveTo(c, y)
        time.sleep(0.1)


def coords():
    time.sleep(2)
    print(pyautogui.position())



#coords()
test()