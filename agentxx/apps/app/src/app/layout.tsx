"use client";

import "./globals.css";
import "@copilotkit/react-core/v2/styles.css";

import { CopilotKit } from "@copilotkit/react-core";
import { ThemeProvider } from "@/hooks/use-theme";

export default function RootLayout({
  children,
}: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="zh">
      <head>
        <meta charSet="UTF-8" />
        <meta name="referrer" content="never" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>冬瓜 • agentxx</title>
      </head>

      <body className={`antialiased`}>
        <ThemeProvider>
          <CopilotKit
            enableInspector={false}
            runtimeUrl="/api/copilotkit"
            agent="agentxx"
          // a2ui={ theme } // Custom theme for A2UI, check @/lib/a2ui-theme.css
          >
            {children}
          </CopilotKit>
        </ThemeProvider>
      </body>
    </html>
  );
}
