import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "export",
  logging: false,
  serverExternalPackages: ["@copilotkit/runtime"],
};

export default nextConfig;
