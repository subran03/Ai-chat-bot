import urllib.request

url = "http://localhost:8000"

try:
    response = urllib.request.urlopen(url)
    data = response.read().decode()

    print("Server response:")
    print(data)

except Exception as e:
    print("Could not connect to server.")
    print("Error:", e)