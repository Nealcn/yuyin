import serial, time
for i in range(5):
    try:
        s = serial.Serial('COM3', 115200, timeout=1)
        s.close()
        print(f'COM3 opened and closed (attempt {i+1})')
        break
    except serial.SerialException as e:
        print(f'Attempt {i+1}: {e}')
    time.sleep(2)
