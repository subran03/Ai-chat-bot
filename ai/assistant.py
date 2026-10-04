import os
from openai import OpenAI
client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY"))
def get_response(message):
    response = client.responses.create(
        model=os.environ.get("OPENAI_MODEL", "gpt-6-luna"),
        input=message
    )
    return response.output_text