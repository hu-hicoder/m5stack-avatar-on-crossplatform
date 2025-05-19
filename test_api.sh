#!/bin/bash

# HTTPリクエスト用シンプルスクリプト
# 使用方法: ./test_api.sh [コマンド]

# サーバーのベースURL
BASE_URL="http://localhost:8080"

# 現在の表情を取得
function get_expression() {
  echo "現在の表情を取得中..."
  curl -s ${BASE_URL}/expression
  echo ""
}

# 表情を設定
function set_expression() {
  local exp=$1
  echo "表情を設定: $exp"
  curl -s -X POST -H "Content-Type: application/json" -d "{\"expression\":\"$exp\"}" ${BASE_URL}/expression
  echo ""
}

# コマンドライン引数に基づいて処理
case "$1" in
  "get")
    get_expression
    ;;
  "set")
    if [ -z "$2" ]; then
      echo "表情を指定してください (例: default, happy, sad, angry, cry, gangimari)"
      exit 1
    fi
    set_expression "$2"
    ;;
  *)
    echo "使用方法:"
    echo "  ./test_api.sh get                  - 現在の表情を取得"
    echo "  ./test_api.sh set <表情名>         - 表情を設定"
    echo "  表情名: default, happy, sad, angry, cry, gangimari"
    ;;
esac
