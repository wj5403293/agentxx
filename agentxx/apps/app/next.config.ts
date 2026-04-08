import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "standalone",
  logging: false,
  serverExternalPackages: ["@copilotkit/runtime"],
};

export default nextConfig;
