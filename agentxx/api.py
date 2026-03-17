from contextlib import asynccontextmanager
from fastapi.staticfiles import StaticFiles
import langchain
langchain.verbose = False

from dotenv import load_dotenv
load_dotenv()

# pylint: disable=wrong-import-position
from fastapi import FastAPI
import uvicorn
from graph import graph

def init_agent():
    return graph

app:FastAPI = None

@asynccontextmanager
async def lifespan(app: FastAPI):
    app.mount("/", StaticFiles(directory="./agent-chat-ui/out/", html=True), name="lumenxs")
    yield

app = FastAPI(lifespan=lifespan)

# add new route for health check
@app.get("/health")
def health():
    """Health check."""
    return {"status": "ok"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "main:app", 
        host="0.0.0.0",
        port=7777,
        reload=False,
        workers=1,
    )
