from flask import Flask, render_template
from flask_socketio import SocketIO
import eventlet
eventlet.monkey_patch()
import serial
import json

app = Flask(__name__)
socketio = SocketIO(app)

config = {
    'listen': '0.0.0.0',
    'port': 6262,
    'servers': {
        'test': {
            'console': '/dev/ttyUSB0',
            'management': '/dev/ttyACM0'
        }
    }
}

def console_worker(port, name):
    while True:
        serial_output = port.read(1024)
        if (serial_output):
            socketio.emit('output', {
                'server': name,
                'data': serial_output.decode('utf-8', errors='replace')
            })
        eventlet.sleep(0.04)

def management_worker(port, name):
    while True:
        serial_output = port.readline()
        while port.in_waiting:
            serial_output = port.readline()
        socketio.emit('status', {
            'server': name,
            'status': json.loads(serial_output)
        })
        eventlet.sleep(0.1)

servers = {}
for name, s in config['servers'].items():
    console = serial.Serial(s['console'], 115200, timeout=0, exclusive=True)
    management = serial.Serial(s['management'], 115200, exclusive=True)
    servers[name] = {
        'console': console,
        'management': management
    }

    eventlet.spawn(console_worker, console, name)
    eventlet.spawn(management_worker, management, name)

@app.route('/')
def index():
    return render_template('index.html', servers=config['servers'])

@app.route('/server/<name>')
def server(name):
    return render_template('server.html', servers=config['servers'], current=name)

@socketio.on('input')
def handle_json(msg):
    name = msg['server']
    server = servers[name]
    server['console'].write(msg['data'].encode('utf-8'))

@socketio.on('action')
def handle_action(msg):
    name = msg['server']
    server = servers[name]
    action = {'action': msg['action']}
    out = json.dumps(action) + '\r'
    server['management'].write(out.encode('utf-8'))

if __name__ == '__main__':
    socketio.run(app, host=config['listen'], port=config['port'])
