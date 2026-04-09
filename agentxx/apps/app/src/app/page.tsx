"use client";

import { ExampleLayout } from "@/components/example-layout";
import { ExampleCanvas } from "@/components/example-canvas";
import { useGenerativeUIExamples, useSuggestionExamples } from "@/hooks";

import dynamic from 'next/dynamic'
import { useAgent } from "@copilotkit/react-core/v2";

// 不直接导入，用动态导入禁用SSR
const CopilotChat = dynamic(
  () => import('@copilotkit/react-core/v2').then(mod => mod.CopilotChat),
  {
    ssr: false,
    loading: () => (
      <>
        <div className="xx_column" style={{ flex: 1, paddingTop: '30px' }}>
          <ul className="xx_ul" id="list">
            <li className="xx_li_notransform" style={{ display: 'flex', flexDirection: 'column', justifyContent: 'center', alignItems: 'center' }}>
              <div style={{ display: 'flex', flexDirection: 'column', justifyContent: 'center', alignItems: 'center', marginBottom: '10px' }}>
                <svg className="pl2" viewBox="0 0 128 128" width="128px" height="128px">
                  <g fill="var(--primary)">
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="0" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="44" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="88" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                  </g>
                  <g fill="var(--primary)" mask="url(#pl-mask)">
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="0" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="44" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                    <g className="pl2__rect-g">
                      <rect className="pl2__rect" rx="8" ry="8" x="88" y="128" width="40" height="24" transform="rotate(180)"></rect>
                    </g>
                  </g>
                </svg>
              </div>
              <span className="xx_textMain">正在加载...</span>
            </li>
          </ul>
        </div>
      </>
    )
  }
)
export default function HomePage() {
  const { agent } = useAgent({
    agentId: "agentxx",
  });
  useGenerativeUIExamples();
  useSuggestionExamples();

  return (
    <ExampleLayout
      chatContent={
        <CopilotChat
          labels={{
            chatInputPlaceholder: "聊点什么...",
            chatInputToolbarStartTranscribeButtonLabel: "开始语音转文字",
            chatInputToolbarCancelTranscribeButtonLabel: "取消语音转文字",
            chatInputToolbarFinishTranscribeButtonLabel: "完成语音转文字",
            chatInputToolbarAddButtonLabel: "添加文件",
            chatInputToolbarToolsButtonLabel: "工具",
            assistantMessageToolbarCopyCodeLabel: "复制代码",
            assistantMessageToolbarCopyCodeCopiedLabel: "代码已复制",
            assistantMessageToolbarCopyMessageLabel: "复制消息",
            assistantMessageToolbarThumbsUpLabel: "点赞",
            assistantMessageToolbarThumbsDownLabel: "点踩",
            assistantMessageToolbarReadAloudLabel: "朗读",
            assistantMessageToolbarRegenerateLabel: "重新回答",
            userMessageToolbarCopyMessageLabel: "复制消息",
            userMessageToolbarEditMessageLabel: "编辑消息",
            chatDisclaimerText: "agentxx By coolight/boolxx",
            chatToggleOpenLabel: "打开聊天窗口",
            chatToggleCloseLabel: "关闭聊天窗口",
            modalHeaderTitle: "助理",
            welcomeMessageText: "有什么我能帮你的吗",
          }}
          input={{
            autoFocus: true,
            onAddFile: () => { },
            disclaimer: () => null, className: "pb-6",
          }} />
      }
      appContent={<ExampleCanvas />}
    />
  );
}
