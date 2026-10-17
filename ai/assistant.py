
import requests


def get_response(message):
    """Generate a reply using the local Gemma model through Ollama."""

    url = "http://localhost:11434/api/chat"

    payload = {
        "model": "gemma3:4b",
        "messages": [
            {"role": "user", "content": message}
        ],
        "stream": False
    }

    response = requests.post(url, json=payload, timeout=120)
    response.raise_for_status()

    data = response.json()
    return data["message"]["content"].strip()
