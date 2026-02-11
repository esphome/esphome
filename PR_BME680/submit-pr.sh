#!/bin/bash
# ESPHome BME680 PR 提交腳本

echo "========================================"
echo "  ESPHome BME680 PR 提交"
echo "========================================"
echo ""
echo "此腳本將幫助你提交 BME680 感測器驅動 PR"
echo ""

# 檢查是否已 Fork
echo "1. 請確認你已經 Fork 了 ESPHome:"
echo "   https://github.com/ssiweifnag/esphome"
echo ""
read -p "按 Enter 繼續..."

# Clone fork
echo ""
echo "2. Clone 你的 Fork..."
cd ~
if [ -d "esphome" ]; then
    echo "   目錄已存在，使用現有版本"
    cd esphome
    git pull
else
    echo "   Clone 中..."
    gh repo clone esphome esphome
    cd esphome
fi

# 建立分支
echo ""
echo "3. 建立分支..."
BRANCH_NAME="add-bme680-sensor-$(date +%Y%m%d)"
git checkout -b $BRANCH_NAME
echo "   分支名稱: $BRANCH_NAME"

# 複製檔案
echo ""
echo "4. 複製 BME680 元件..."
mkdir -p components/bme680/
cp ~/openclaw/workspace/PR_BME680/* components/bme680/
echo "   完成!"

# 提交
echo ""
echo "5. 提交變更..."
git add components/bme680/
git commit -m "Add BME680 environmental sensor component

- Add temperature, pressure, humidity, and gas resistance sensors
- Support I2C communication
- Include configuration examples
- Add documentation

Closes #XXXX"

# Push
echo ""
echo "6. Push 到你的 Fork..."
git push -u origin $BRANCH_NAME

echo ""
echo "========================================"
echo "  下一步"
echo "========================================"
echo ""
echo "1. 前往: https://github.com/esphome/esphome/compare/main...ssiweifnag:$BRANCH_NAME"
echo "2. 點擊 'Create Pull Request'"
echo "3. 填寫 PR 標題和描述"
echo "4. 點擊 'Create Pull Request'"
echo ""
echo "PR 標題建議: Add BME680 environmental sensor component"
echo ""
echo "完成後等待維護者回覆!"
