/**
 * @file    sl_page.h
 * @brief   SlateUI 椤甸潰鎶借薄鍩虹被
 *
 * 鏈ā鍧楀畾涔変簡 SlateUI 椤甸潰鐨勬牳蹇冩暟鎹粨鏋勪笌鐢熷懡鍛ㄦ湡鍥炶皟锛?
 * 绫讳技 碌C/OS-II 鐨勪换鍔℃帶鍒跺潡 (TCB) 璁捐鐞嗗康锛?
 *   路 姣忎釜椤甸潰閮芥槸涓€涓嚜鍖呭惈鐨勭姸鎬佹満锛屾嫢鏈夊垵濮嬪寲銆佺粯鍒躲€?
 *     浜嬩欢澶勭悊銆侀€€鍑哄洓涓敓鍛藉懆鏈熷洖璋冦€?
 *   路 椤甸潰鍒囨崲鐢变簨浠堕┍鍔紝绠＄悊鍣ㄩ€氳繃璋冪敤椤甸潰鐨?proc() 鍒嗗彂浜嬩欢锛?
 *     褰?proc() 杩斿洖 1 鏃堕〉闈㈤€€鍑猴紝绠＄悊鍣ㄨ嚜鍔ㄨ繑鍥炰笂涓€椤甸潰銆?
 *   路 椤甸潰鏁版嵁閫氳繃 void *data 瀹炵幇澶氭€侊紝鐢ㄦ埛鍙湪娲剧敓椤甸潰涓?
 *     寮哄埗杞崲涓哄叿浣撶被鍨嬨€?
 *   路 椤甸潰闂翠紶鍙傞€氳繃 void *arg 瀹炵幇锛岀敱 sl_page_enter_with() 璁剧疆銆?
 *
 * 椤甸潰鐢熷懡鍛ㄦ湡锛?
 *   sl_page_enter() 鈫?init() 鈫?[draw() + proc()] 寰幆 鈫?exit()
 */

#ifndef SL_PAGE_H
#define SL_PAGE_H

#include "sl_event.h"

typedef struct sl_Page sl_Page;

/* ======================== 椤甸潰鍥炶皟鍑芥暟绫诲瀷瀹氫箟 ======================== */

/**
 * @brief  椤甸潰浜嬩欢澶勭悊鍥炶皟鍑芥暟绫诲瀷
 * @param  self   鎸囧悜褰撳墠椤甸潰瀹炰緥
 * @param  event  寰呭鐞嗙殑浜嬩欢鎸囬拡锛堟寜閿€佸畾鏃跺櫒绛夛級
 * @retval 0      浜嬩欢宸插鐞嗭紝椤甸潰缁х画淇濇寔娲昏穬
 * @retval 1      璇锋眰閫€鍑哄綋鍓嶉〉闈紙绠＄悊鍣ㄥ皢寮瑰嚭椤甸潰鏍堬級
 * @note   姝ゅ洖璋冨湪涓诲惊鐜笂涓嬫枃涓璋冪敤锛屽簲淇濇寔闈為樆濉?
 */
typedef int  (*sl_PageProc)(sl_Page *self, const sl_Event *event);

/**
 * @brief  椤甸潰鍒濆鍖栧洖璋冨嚱鏁扮被鍨?
 * @param  self  鎸囧悜褰撳墠椤甸潰瀹炰緥
 * @note   椤甸潰琚帹鍏ユ爤鏃惰皟鐢ㄤ竴娆★紝鐢ㄤ簬鍒濆鍖栭〉闈㈢姸鎬佸拰鎺т欢
 */
typedef void (*sl_PageInit)(sl_Page *self);

/**
 * @brief  椤甸潰缁樺埗鍥炶皟鍑芥暟绫诲瀷
 * @param  self  鎸囧悜褰撳墠椤甸潰瀹炰緥
 * @note   搴旈€氳繃 sl_disp_xxx 绯诲垪鍑芥暟鎿嶄綔鏄惧瓨缂撳啿鍖猴紝
 *         绂佹鍦ㄦ鍥炶皟涓皟鐢ㄧ墿鐞嗗埛鏂板嚱鏁帮紙sl_disp_flush锛?
 */
typedef void (*sl_PageDraw)(sl_Page *self);

/**
 * @brief  椤甸潰閫€鍑哄洖璋冨嚱鏁扮被鍨?
 * @param  self  鎸囧悜褰撳墠椤甸潰瀹炰緥
 * @note   椤甸潰浠庢爤涓脊鍑烘椂璋冪敤锛岀敤浜庨噴鏀鹃〉闈㈡寔鏈夌殑璧勬簮
 */
typedef void (*sl_PageExit)(sl_Page *self);

/**
 * @brief  椤甸潰 Presenter 鍥炶皟鍑芥暟绫诲瀷
 * @param  evt   鎸囧悜 UI 璇箟浜嬩欢鐨勫彧璇绘寚閽?
 * @param  page  鎸囧悜浜х敓浜嬩欢鐨勯〉闈㈠疄渚?
 * @note   Presenter 鏄墠鍚庡彴鍒嗙鐨勬ˉ姊侊細
 *         鍓嶅彴锛堟帶浠?椤甸潰锛変骇鐢?UI 璇箟浜嬩欢锛?
 *         鍚庡彴锛圥resenter锛夋秷璐逛簨浠跺苟鎵ц涓氬姟閫昏緫銆?
 *         姝ゅ洖璋冪敱 sl_ui_event_post 瑙﹀彂銆?
 */
typedef void (*sl_PagePresenter)(const sl_UiEvent *evt, sl_Page *page);

/* ======================== 椤甸潰缁撴瀯浣撳畾涔?======================== */

/**
 * @brief  椤甸潰缁撴瀯浣擄紙绫讳技 碌C/OS-II 鐨?OS_TCB锛?
 *
 * 姣忎釜椤甸潰瀹炰緥鍖呭惈鐢熷懡鍛ㄦ湡鍥炶皟銆丳resenter銆佺鏈夋暟鎹拰鍏ュ彛鍙傛暟銆?
 * 椤甸潰绠＄悊鍣ㄩ€氳繃姝ょ粨鏋勪綋缁熶竴璋冨害鎵€鏈夐〉闈€?
 */
struct sl_Page {
    const char       *name;       /**< 椤甸潰鍚嶇О锛堣皟璇曠敤锛屽彲涓?NULL锛?*/
    sl_PageInit       init;       /**< 鍒濆鍖栧洖璋冿紝椤甸潰鍏ユ爤鏃惰皟鐢?*/
    sl_PageDraw       draw;       /**< 缁樺埗鍥炶皟锛岄渶瑕侀噸缁樻椂璋冪敤 */
    sl_PageProc       proc;       /**< 浜嬩欢澶勭悊鍥炶皟锛屾瘡甯т簨浠跺垎鍙戞椂璋冪敤 */
    sl_PageExit       exit;       /**< 閫€鍑哄洖璋冿紝椤甸潰鍑烘爤鏃惰皟鐢?*/
    sl_PagePresenter  presenter;  /**< Presenter 鍥炶皟锛屽鐞?UI 璇箟浜嬩欢 */
    void             *data;       /**< 椤甸潰绉佹湁鏁版嵁鎸囬拡锛堝鎬侊紝鐢ㄦ埛鑷杞崲锛?*/
    void             *arg;        /**< 椤甸潰鍏ュ彛鍙傛暟锛堢敱 sl_page_enter_with 璁剧疆锛?*/
};

#endif


