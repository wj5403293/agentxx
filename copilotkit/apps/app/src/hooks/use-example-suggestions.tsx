import { useConfigureSuggestions } from "@copilotkit/react-core/v2";

export const useSuggestionExamples = () => {
  useConfigureSuggestions({
    suggestions: [
      {
        title: "切换主题",
        message: "使用工具`toggleTheme`切换页面主题",
      },
    ],
    available: "always",
  });
};
