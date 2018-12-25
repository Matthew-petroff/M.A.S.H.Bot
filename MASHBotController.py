#!/usr/bin/env python3
# By TheMas3212
# the pyserial is needed
import serial
import string
import time
import sys
import struct
import os.path, os
from serial import SerialException

DEBUG = True

# Flags
powerFlag = b'\xfd'
completionFlag = b'\xfa'
stylesFlag = b'\xfc'
pollFlag = b'\xf0'
killFlag = b'\xfe'
confirmationFlag = b'\xff'

# Handshake Settings
waitTime = 2 

# Serial Settings
# serialPort = 'COM4'
serialPort = None
serialBaud = 115200
serialTimeout = float(10)
serialReset = False

# Delay
delayFrame = 5 # Serial Init Delay
frameDelay = 1/60 # Time Per Frame

# Script will look in this path for input data
level_path = 'v5'

inputStruct = struct.Struct('sBBBBBBs') # (0xfc X X Y Y Z Z 0xfa)
# inputStruct = struct.Struct('sBBBs') # (0xfc X Y Z 0xfa)

def processTASFile(file):
    # fullPath = os.path.join(pathToFiles, file)
    fullPath = file
    buffer = []
    with open(fullPath, 'r') as f:
        for line in f.readlines():
            if line[0] != '|':
                continue
            x,y,z = line[16:25].split(' ')
            x,y,z = int(x),int(y),int(z)
            frame = inputStruct.pack(stylesFlag, x, x, y, y, z, z, completionFlag)
            # frame = inputStruct.pack(stylesFlag, x, y, z, completionFlag)
            buffer.append(frame)
    return buffer

def readLevels(root):
    global start_buffer
    global credits_buffer
    global levels
    global level_list
    start_buffer = None
    credits_buffer = None
    levels = {}
    global buffers
    level_list = []
    for x in os.listdir(root):
        path = os.path.join(root, x)
        if os.path.isdir(path):
            level_list.append(path)
            print('Found Level: {}'.format(x))
        if os.path.isfile(path):
            if 'start' in path.lower():
                start_buffer = processTASFile(path)
            elif 'credit' in path.lower():
                credits_buffer = processTASFile(path)
    level_list.sort()
    for level in level_list:
        level_data = processLevel(level)
        level_data2 = {
            'start': level_data[0],
            'end': level_data[1],
            'polls': level_data[2]
            }
        levels[level] = level_data2
    assert start_buffer != None, 'Failed to find game start input data!'
    assert credits_buffer != None, 'Failed to find credits input data!'

def processLevel(level_root):
    # print('Processing: {}'.format(level_root))
    level_parts = {}
    level_exit_id = '0'
    for x in os.listdir(level_root):
        path = os.path.join(level_root, x)
        if os.path.isdir(path):
            continue
        id, name = x.split('_')
        name = name[:-4]
        id2 = id.translate(str.maketrans('', '', ''.join(set(string.printable) - set(string.digits))))
        if int(id2) > int(level_exit_id):
            level_exit_id = id2
        level_parts[id] = (id, id2, name, processTASFile(path))
    level_start = level_parts['01']
    level_exit = level_parts[level_exit_id]
    id, id2, name, level_start_buffer = level_start
    id, id2, name, level_exit_buffer = level_exit
    assert len(level_start_buffer) > 0
    assert len(level_exit_buffer) > 0
    level_polls = []
    for x in range(2,int(level_exit_id)):
        poll_choices = []
        id = str(x).rjust(2, '0')
        if id == '06' and os.path.split(level_root)[1] == 'Level9-4':
            # print('Bypass for 9-4: \'NUCLEAR-WINTRY-DEAD-LION\'')
            poll_choices.append(level_parts['06'])
            poll_choices.append(level_parts['06'])
            poll_choices.append(level_parts['06'])
            poll_choices.append(level_parts['06'])
        else:
            for y in 'abcd':
                id2 = id + y
                poll_choices.append(level_parts[id2])
        level_polls.append(poll_choices)
    return (level_start, level_exit, level_polls)

# Process input data
readLevels(level_path)

if serialPort == None:
    try:
        serialPort = sys.argv[1]
    except:
        print('Usage {} <Serial Port>'.format(sys.argv[0]))
        sys.exit(1)
    ser = None

def pollChoice(options):
    if len(options) > 26:
        print('Too Many Options')
        sys.exit()
    num = 97
    for opt in options:
        print(chr(num) + ': ' + opt[1])
        num += 1
    while True:
        choice = input().lower()
        if len(choice) != 1:
            print('Bad Choice')
            continue
        choice_num = ord(choice[0])-97
        if choice_num >= len(options) or choice_num < 0:
            print('Bad Choice')
            continue
        return options[choice_num]

def startGame(bot):
    bot.sendBuffer((start_buffer, 'Game Start'))

def showCredits(bot):
    bot.sendBuffer((credits_buffer, 'Game Credits'))

def playFullRun(bot):
    startGame(bot)
    for level in level_list:
        playLevel(bot, level)
    showCredits(bot)

def playLevel(bot, level):
    level_data = levels[level]
    start_buffer = level_data['start']
    end_buffer = level_data['end']
    polls = level_data['polls']
    bot.sendBuffer((start_buffer, 'Starting Level: {}'.format(level)))
    for poll in polls:
        options = []
        for option in poll:
            options.append((option[3], option[2]))
        bot.sendBuffer(pollChoice(options))
    bot.sendBuffer((end_buffer, 'Leaving Level: {}'.format(level)))

# class DummyBot():
    # def sendBuffer(self, pack):
        # buffer, name = pack
        # print('Sending buffer: {}'.format(name))

# dbot = DummyBot()
# playLevel(dbot, 'v5/Level1-8')
# playFullRun(dbot)

class MASHBot():
    def __init__(self, port, baud, timeout, reset):
        self.ser = None
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.reset = reset

    def initSerial(self):
        try:
            self.ser = serial.Serial(None, baudrate=self.baud, timeout = self.timeout, dsrdtr=self.reset)
            self.ser.port = self.port
            self.ser.open()
            self.ser.dtr = False
            time.sleep(1/20)
            self.ser.dtr = True
            time.sleep(waitTime)
            self.send(killFlag)
            while True:
                data = self.recv(1)
                if data == confirmationFlag: #0xFF
                    print('Arduino > PC Handshake Received')
                    self.send(confirmationFlag) #0xFF
                    print('PC > Arduino Handshake Sent')
                    print('Init Handshake Successful')
                    break
                else:
                    print('bad data', data)
                    # sys.exit(1)
        except SerialException as e:
            print(e.args)
            sys.exit(1)
        return True

    def send(self, data):
        if DEBUG: print('S', data)
        self.ser.write(data)

    def recv(self, c):
        data = self.ser.read(c)
        if DEBUG: print('R', data)
        return data

    def initPower(self):
        print('Sending Power Init Command')
        self.send(powerFlag) #0xFD
        while True:
            data = self.recv(1)
            if data == completionFlag: #0xFA
                print('Arduino Power Init Complete')
                break
            else:
                time.sleep(delayFrame)
        return True

    def deInitPower(self):
        print('Sending Power Shutdown Command')
        self.send(powerFlag) #0xFD
        while True:
            data = self.recv(1)
            if data == completionFlag: #0xFA
                print('Arduino Power Shutdown Complete')
                break
            else:
                time.sleep(delayFrame)
        return True

    def sendBuffer(self, pack):
        buffer, name = pack
        print('Sending buffer: {}'.format(name))
        state = None
        fcount = 0
        for frame in buffer:
            print('level: {} frame: {} data: '.format(name, fcount), frame)
            fcount += 1
            if frame != state:
                state = frame
                self.send(frame)
                time.sleep(frameDelay)
                got_data = False
                while True:
                    if not got_data:
                        data = self.recv(8)
                        got_data = True
                    a = self.recv(1)
                    if a == completionFlag: #0xFA
                        break
                    if a == stylesFlag: #0xFC
                        self.send(frame)
                        got_data = False
                        time.sleep(frameDelay)
                continue
            else:
                time.sleep(frameDelay)

MBot = MASHBot(serialPort, serialBaud, serialTimeout, serialReset)
MBot.initSerial()
time.sleep(delayFrame)
MBot.initPower()
# Level List:
# ['v5/Level1-8', 'v5/Level5-4', 'v5/Level5-9', 'v5/Level6-9', 'v5/Level7-8', 'v5/Level9-4']
# playLevel(MBot, 'v5/Level1-8')
playFullRun(MBot)
MBot.deInitPower()

sys.exit(0)
