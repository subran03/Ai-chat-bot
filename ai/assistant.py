import requests
OLLAMA_URL = "http://127.0.0.1:11434/api/chat"
MODEL = "gemma3:4b"
session = requests.Session()
def get_response(message):
    """Generate a concise reply using local Gemma through Ollama."""
    payload = {
        "model": MODEL,
        "messages": [
            {
                "role": "system",
                "content": (
                    "You are a friendly, helpful female AI assistant. "
                    "Answer the user's question directly and accurately. "
                    "For simple questions, use 1-3 short sentences. "
                    "Avoid unnecessary introductions, repetition, and long lists. "
                    "Give detailed answers only when explicitly requested."
                ),
            },
            {"role": "user", "content": message.strip()},
        ],
        "stream": False,
        "options": {
            "num_predict": 180,
            "temperature": 0.5,
        },
    }

    try:
        response = session.post(
            OLLAMA_URL,
            json=payload,
            timeout=(5, 180),
        )
        response.raise_for_status()

        data = response.json()
        answer = data.get("message", {}).get("content", "").strip()

        if not answer:
            raise RuntimeError(f"Ollama returned an empty answer: {data}")

        return answer

    except requests.RequestException as exc:
        raise RuntimeError(f"Could not get a response from Ollama: {exc}") from exc
