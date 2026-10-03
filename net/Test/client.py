import urllib.request
import urllib.error
import json

url ="http://localhost:8000"

while True:
    message =input("Enter a message: ")

    if message.lower() == "stop":
        print("Client stopped.")
        break

    data ={"message": message}

    json_data =json.dumps(data).encode()

    request =urllib.request.Request(url,data=json_data,headers={"Content-Type": "application/json"},
        method="POST"
    )

    try:
        response =urllib.request.urlopen(request)
        result = response.read().decode()

        print("Server response:")
        print(result)

    except urllib.error.HTTPError as e:
        error = e.read().decode()
        print("Server rejected the request.")
        print("Error:", error)

    except urllib.error.URLError as e:
        print("Could not connect to server.")
        print("Error:", e)

    except Exception as e:
        print("Unexpected error.")
        print("Error:", e)