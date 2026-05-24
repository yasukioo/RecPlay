import * as echarts from "echarts/core";
import { BarChart, GaugeChart, LineChart } from "echarts/charts";
import { GridComponent, TooltipComponent } from "echarts/components";
import ReactEChartsCore from "echarts-for-react/lib/core";
import { SVGRenderer } from "echarts/renderers";

echarts.use([
  LineChart,
  BarChart,
  GaugeChart,
  GridComponent,
  TooltipComponent,
  SVGRenderer,
]);

export { echarts, ReactEChartsCore };
