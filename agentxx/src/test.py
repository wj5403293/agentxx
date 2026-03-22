import asyncio
from fastmcp import Client

client = Client("http://localhost:53023/mcp")

async def call_tool():
    async with client:
        print("call_tool:")
        result = await client.list_tools()
        print(result)

asyncio.run(call_tool())