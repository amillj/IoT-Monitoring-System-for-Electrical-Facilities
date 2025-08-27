import paho.mqtt.client as mqtt
import json
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq
from collections import deque

# MQTT parametri
FLESPI_TOKEN = "flespi_token"  # zamijeni svojim Flespi tokenom
MQTT_BROKER = "mqtt.flespi.io"
MQTT_PORT = 1883
MQTT_TOPIC = "Trafostanica1/Trafo1"
JSON_FIELD = "AccelZ"
JSON_FIELD_TEMP = "Temperature"

# FFT parametri
SAMPLE_RATE = 1000  # Hz
BUFFER_SIZE = 1000
PLOT_FREQ_LIMIT = 500

# Temperaturni prag
TEMP_THRESHOLD = 100  # C

# Buffers
accumulated_data = deque()
temperature_values = deque(maxlen=1000)

# Postavi plot jednom (interaktivni mod)
plt.ion()
fig, ax = plt.subplots()
line, = ax.plot([], [])

temp_text = ax.text(0.95, 0.95, '', transform=ax.transAxes,
                    verticalalignment='top', horizontalalignment='right',
                    fontsize=10, bbox=dict(boxstyle="round",
                                          facecolor="white", alpha=0.7))

alert_text = ax.text(0.96, 0.85, '', transform=ax.transAxes,
                     verticalalignment='top', horizontalalignment='right',
                     fontsize=12, color='red')

ax.set_xlim(0, PLOT_FREQ_LIMIT)
ax.set_ylim(0, 1)
ax.set_xlabel("Frekvencija [Hz]")
ax.set_ylabel("Amplituda")
ax.set_title("Real-time FFT [0-500 Hz]")
ax.grid(True)


def update_fft_plot(data, temperature_avg):
    """Update the FFT plot with new data and temperature info"""
    data = np.array(data)
    
    # Perform FFT
    yf = fft(data)
    xf = fftfreq(BUFFER_SIZE, 1 / SAMPLE_RATE)
    
    # Filter positive frequencies up to limit
    mask = (xf >= 0) & (xf <= PLOT_FREQ_LIMIT)
    xf = xf[mask]
    yf = np.abs(yf[mask]) / BUFFER_SIZE
    
    # Update plot data
    line.set_data(xf, yf)
    ax.set_xlim(0, PLOT_FREQ_LIMIT)
    ax.set_ylim(0, max(yf) * 1.1 if max(yf) > 0 else 1)
    
    # Prikaz temperature i upozorenja
    color = 'red' if temperature_avg >= TEMP_THRESHOLD else 'black'
    temp_text.set_text(f"Avg Temp: {temperature_avg:.1f}°C")
    temp_text.set_color(color)
    
    if temperature_avg >= TEMP_THRESHOLD:
        alert_text.set_text("ALERT! Temperature above threshold.")
    else:
        alert_text.set_text("")
    
    # Set x-axis ticks
    ax.set_xticks(np.arange(0, PLOT_FREQ_LIMIT + 25, 25))
    
    # Refresh plot
    fig.canvas.draw()
    fig.canvas.flush_events()


# MQTT callbacks
def on_connect(client, userdata, flags, rc):
    """Callback for MQTT connection"""
    if rc == 0:
        print("Povezano sa Flespi brokerom.")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Greška pri konekciji. Kod: {rc}")


def on_message(client, userdata, msg):
    """Callback for received MQTT messages"""
    try:
        data = json.loads(msg.payload.decode())
        accel = data.get(JSON_FIELD)
        temp = data.get(JSON_FIELD_TEMP)
        
        # Process accelerometer data
        if isinstance(accel, list):
            accumulated_data.extend(float(val) for val in accel)
        
        # Process temperature data
        if isinstance(temp, (int, float)):
            temperature_values.append(float(temp))
        
        # Update plot when buffer is full
        if len(accumulated_data) >= BUFFER_SIZE:
            buffer = [accumulated_data.popleft() for _ in range(BUFFER_SIZE)]
            temp_avg = np.mean(temperature_values) if temperature_values else 0.0
            update_fft_plot(buffer, temp_avg)
            
    except Exception as e:
        print(f"Greška u poruci: {e}")


# MQTT inicijalizacija
client = mqtt.Client()
client.username_pw_set(FLESPI_TOKEN)
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()
    
    print("MQTT klijent pokrenut. Čeka podatke...")
    print("Pritisnite Ctrl+C za izlaz.")
    
    # Glavna petlja
    while True:
        plt.pause(0.01)
        
except KeyboardInterrupt:
    print("\nZatvaranje...")
    client.loop_stop()
    client.disconnect()
    plt.close('all')
    print("Program završen.")
    
except Exception as e:
    print(f"Greška: {e}")
    client.loop_stop()
    client.disconnect()
    plt.close('all')