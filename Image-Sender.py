import asyncio
import websockets
import numpy as np
from PIL import Image, ImageEnhance
import io
import mss

ESP8266_IP = "IP_ADRESS" # Change with your Esp8266 Ip Adress
ESP8266_PORT = 81
SCREEN_WIDTH = 280
SCREEN_HEIGHT = 158
JPEG_QUALITY = 25
TARGET_FPS = 10

class ScreenCapturer:
    def __init__(self):
            self.sct = mss.mss()
            self.monitor = self.sct.monitors[1]
            self.capture_method = "MSS"
    
    def capture(self):
            sct_img = self.sct.grab(self.monitor)
            return Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX")

async def send_screen_and_audio():
    uri = f"ws://{ESP8266_IP}:{ESP8266_PORT}"
    
    capturer = ScreenCapturer()
    print(f"Screencapture: {capturer.capture_method}")
        
    while True: 
        try:
            print(f"Connecting to ESP8266: {uri}")
            async with websockets.connect(uri, ping_interval=None) as websocket:
                print("Connection Succes! Stream is starting...")
                
                frame_count = 0
                
                while True:
                    img = capturer.capture()
                    if img is None:
                        await asyncio.sleep(0.01)
                        continue
                    
                    resized_img = img.resize((SCREEN_WIDTH, SCREEN_HEIGHT), Image.Resampling.LANCZOS)
                    
                    enhancer = ImageEnhance.Sharpness(resized_img)
                    resized_img = enhancer.enhance(1.2)
                    
                    buffer = io.BytesIO()
                    resized_img.save(buffer, format="JPEG", quality=JPEG_QUALITY, optimize=True)
                    jpeg_data = buffer.getvalue()
                    
                    await websocket.send(f"JPEG_FRAME_SIZE:{len(jpeg_data)}")
                    
                    CHUNK_SIZE = 1024*4
                    for i in range(0, len(jpeg_data), CHUNK_SIZE):
                        chunk = jpeg_data[i:i+CHUNK_SIZE]
                        await websocket.send(chunk)
                                        
                    await asyncio.sleep(1 / TARGET_FPS)

        except websockets.exceptions.ConnectionClosed as e:
            print(f"Disconnected: {e}. try to connect...")
            await asyncio.sleep(2)
        except KeyboardInterrupt:
            print("\nClosing...")
            break
        except Exception as e:
            print(f"Error: {e}")
            import traceback
            traceback.print_exc()
            await asyncio.sleep(2)

if __name__ == "__main__":
    print("=" * 60)
    print("Tiny Display Project ")
    print("=" * 60)
    print(f"Target: {ESP8266_IP}:{ESP8266_PORT}")
    print(f"Video: {SCREEN_WIDTH}x{SCREEN_HEIGHT} @ {TARGET_FPS} FPS")
    print(f"JPEG Quality: {JPEG_QUALITY}")
    print("=" * 60)
    print("\nNeccesary Libraries:")
    print("  pip install websockets pillow numpy")
    print("  pip install mss")
    print("=" * 60)
    
    try:
        asyncio.run(send_screen_and_audio())
    except KeyboardInterrupt:
        print("\nProgram is closed.")
