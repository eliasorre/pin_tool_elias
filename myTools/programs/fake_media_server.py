import os
import re
import sys
import time
import threading
import requests
from flask import Flask, request, send_file, Response

app = Flask(__name__)

stop_threads = False

@app.route('/video')
def video():
    range_header = request.headers.get('Range', None)
    print("Triggered")
    if not range_header:
        return send_file('media/example_video.mp4')

    size = os.path.getsize('media/example_video.mp4')
    byte1, byte2 = 0, None

    m = re.search('(\d+)-(\d*)', range_header)
    g = m.groups()

    byte1 = int(g[0])
    if g[1]:
        byte2 = int(g[1])

    length = size - byte1
    if byte2 is not None:
        length = byte2 + 1 - byte1

    data = None
    with open('sample.mp4', 'rb') as f:
        f.seek(byte1)
        data = f.read(length)

    rv = Response(data, 206, mimetype='video/mp4', content_type='video/mp4', direct_passthrough=True)
    rv.headers.add('Content-Range', 'bytes {0}-{1}/{2}'.format(byte1, byte1 + length - 1, size))

    return rv

def run_server():
    app.run(host='0.0.0.0', port=8000, threaded=True)


def emulate_stream(ticks):
    video = requests.get("http://127.0.0.1:8000/video", stream=True)  # Added stream=True to get content iteratively
    time.sleep(2)  # Give the server some time to start
    
    with open('received_video.mp4', 'wb') as f:  # Open a file to save the video
        for chunk in video.iter_content(chunk_size=8192):  # Iterate over the content in chunks
            f.write(chunk)
    
    os._exit(1)

if __name__ == '__main__':
    print(sys.argv)
    ticks = int(sys.argv[1])
    print("Number of ticks: ", ticks)
    server_thread = threading.Thread(target=run_server)
    server_thread.start()
    emulate_stream(ticks)