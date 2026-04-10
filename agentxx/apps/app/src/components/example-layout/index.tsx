"use client";

import { useAgent } from "@copilotkit/react-core/v2";
import { ReactNode } from "react";
import { Button } from "@/components/ui/button";
import { useFrontendTool } from "@copilotkit/react-core";

interface ExampleLayoutProps {
  chatContent: ReactNode;
}

export function ExampleLayout({ chatContent }: ExampleLayoutProps) {
  const { agent } = useAgent({
    agentId: "agentxx",
  });

  function newChatThread() {
    agent.abortRun();
    agent.setMessages([]);
  }

  useFrontendTool({
    name: "newChatThread",
    description: "开启新的对话",
    handler: newChatThread,
  });

  return (
    <div className="h-full flex flex-row">
      <div className="flex-row">
        <Button
          onClick={newChatThread}
          className={`fixed top-4 left-4 z-50 flex rounded-full`}>
          新对话
        </Button>
      </div>

      {/* Chat Content */}
      <div className={`max-h-full overflow-y-auto flex-1 max-lg:px-4`}>
        {chatContent}
      </div>
    </div>
  );
}
