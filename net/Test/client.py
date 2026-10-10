import json
import sys
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT))
from voice.tts import speak
SERVER_URL = "http://localhost:8000"
def send_message(message):
    """Send a message to the chatbot server and return its response."""
    data = json.dumps({"message": message}).encode("utf-8")
    request = Request(
        SERVER_URL,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    with urlopen(request, timeout=60) as response:
        return json.loads(response.read().decode("utf-8"))
def main():
    print("Voice-enabled chatbot client")
    print("Type 'exit' to quit.\n")
    while True:
        try:
            message = input("Enter a message: ").strip()
            if message.lower() == "exit":
                print("Chatbot client closed.")
                break
            if not message:
                print("Please enter a message.")
                continue
            result = send_message(message)
            if "error" in result:
                print("Server error:", result["error"])
                continue
            answer = result.get("response", "")
            if not isinstance(answer, str) or not answer.strip():
                print("The server returned an empty response.")
                continue
            print("\nChatbot:", answer)
            print("Speaking response...")
            latency = speak(answer)
            if latency is not None:
                print(f"Speech completed. TTS latency: {latency:.3f} seconds")
            else:
                print("Response received, but speech playback failed.")

            print()
        except HTTPError as error:
            print(f"HTTP error: {error.code} - {error.reason}")
        except URLError as error:
            print("Could not connect to the server.")
            print("Make sure net/server.py is running.")
            print("Details:", error.reason)
        except KeyboardInterrupt:
            print("\nChatbot client stopped.")
            break
        except Exception as error:
            print("An error occurred:", error)
if __name__ == "__main__":
    main()