import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  experimental: {
    allowedDevOrigins: ["http://192.168.0.24:3000"],
  },
};

export default nextConfig;
