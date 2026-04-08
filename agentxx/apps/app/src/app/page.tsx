"use client";

import { ExampleLayout } from "@/components/example-layout";
import { ExampleCanvas } from "@/components/example-canvas";
import { useGenerativeUIExamples, useExampleSuggestions } from "@/hooks";

import dynamic from 'next/dynamic'
// 不直接导入，用动态导入禁用SSR
const CopilotChat = dynamic(
  () => import('@copilotkit/react-core/v2').then(mod => mod.CopilotChat),
  { ssr: false, loading: () => <div>Loading...</div> }
)
export default function HomePage() {
  useGenerativeUIExamples();
  useExampleSuggestions();

  return (
    <ExampleLayout
      chatContent={
        <CopilotChat input={{ disclaimer: () => null, className: "pb-6" }} />
      }
      appContent={<ExampleCanvas />}
    />
  );
}
