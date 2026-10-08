import { defineConfig } from "vitest/config";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
export default defineConfig({
    plugins: [react(), tailwindcss()],
    build: {
        rollupOptions: {
            output: {
                manualChunks: function (id) {
                    if (id.indexOf("node_modules") === -1) {
                        return undefined;
                    }
                    if (id.indexOf("/node_modules/echarts/") !== -1) {
                        if (id.indexOf("/charts/") !== -1) {
                            return "echarts-charts";
                        }
                        if (id.indexOf("/components/") !== -1) {
                            return "echarts-components";
                        }
                        if (id.indexOf("/renderers/") !== -1) {
                            return "echarts-renderers";
                        }
                        return "echarts-core";
                    }
                    if (id.indexOf("/node_modules/echarts-for-react/") !== -1) {
                        return "echarts-react";
                    }
                    if (id.indexOf("/node_modules/zrender/") !== -1) {
                        return "zrender";
                    }
                    return undefined;
                },
            },
        },
    },
    server: {
        host: "127.0.0.1",
        port: 5173,
        proxy: {
            "/api": {
                target: "http://127.0.0.1:8080",
                ws: true,
            },
        },
    },
    test: {
        environment: "node",
        include: ["src/**/*.test.ts"],
    },
});
