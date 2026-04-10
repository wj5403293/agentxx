import { z } from "zod";
import { useTheme } from "@/hooks/use-theme";

// CopilotKit imports
import {
  useFrontendTool,
  useDefaultRenderTool,
} from "@copilotkit/react-core/v2";

import { ToolReasoning } from "@/components/tool-rendering";

export const useGenerativeUIExamples = () => {
  const { theme, setTheme } = useTheme();

  // ----------------------------------------------------------
  // 3. Default Tool Rendering (backend tool UI)
  //    https://docs.copilotkit.ai/langgraph/generative-ui/backend-tools
  // ----------------------------------------------------------
  const ignoredTools = [
    // generate_form is rendered by A2UI's declarative surface system, not as a tool call
    "generate_form",
    // log_a2ui_event is an internal A2UI event tracker, not meaningful to display to users
    "log_a2ui_event",
  ];
  useDefaultRenderTool({
    render: ({ name, status, parameters }) => {
      if (ignoredTools.includes(name)) return <></>;
      return <ToolReasoning name={name} status={status} args={parameters} />;
    },
  });

  // ----------------------------------------------------------
  // 4. Frontend Tools (direct frontend state manipulation)
  //    https://docs.copilotkit.ai/langgraph/frontend-actions
  // ----------------------------------------------------------
  useFrontendTool(
    {
      name: "toggleTheme",
      description: "Frontend tool for toggling the theme of the app.",
      parameters: z.object({}),
      handler: async () => {
        setTheme(theme === "dark" ? "light" : "dark");
      },
    },
    [theme, setTheme],
  );
};
