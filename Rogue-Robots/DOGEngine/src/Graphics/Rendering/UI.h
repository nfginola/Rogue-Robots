#include <string>
#include <time.h>
#include <functional>
#include "../RHI/DX12/D2DBackend_DX12.h"
#include "../RHI/RenderDevice.h"

class UIElement;
class RenderDevice;
class Swapchain;
class UIScene;


class UI
{
   public:
   std::unique_ptr<DOG::gfx::D2DBackend_DX12> m_d2d; //The thing that renders everything
   UINT m_width, m_height;
   UI(DOG::gfx::RenderDevice* m_rd, DOG::gfx::Swapchain* sc, u_int maxFramesInFlight, HWND hwnd);
   ~UI();
   void DrawUI();
   void ChangeUIscene(UINT sceneID);
   UINT AddUIlEmentToScene(UINT sceneID, std::unique_ptr<UIElement> element);
   UINT GenerateUID();
   UINT AddScene();
   UINT QuerryScene(UINT sceneID);
   void RemoveScene(UINT sceneID);

   private:
   std::vector<std::unique_ptr<UIScene>> m_scenes;
   UINT m_currsceneID, m_currsceneIndex;
   bool m_visible;
   void BuildMenuUI();
   void BuildGameUI();
   std::vector<UINT> m_generatedIDs;

   ComPtr<IDWriteTextFormat> m_btextformat;
};

class UIScene
{
   public:
   UIScene(UINT id);
   ~UIScene() = default;
   UINT m_ID;
   std::vector<std::unique_ptr<UIElement>> m_scene;

};

class UIElement
{
   public:
   UIElement(UINT id);
   virtual void Draw(DOG::gfx::D2DBackend_DX12& m_d2d) = 0;
   virtual void Update(DOG::gfx::D2DBackend_DX12& m_d2d);

   virtual ~UIElement();
   UINT m_ID;
   private:
};

class UIButton : public UIElement
{
   public:
   bool pressed;
   UIButton(float x, float y, float width, float height, std::wstring text, std::function<void(void)> callback, UINT id);
   void Draw(DOG::gfx::D2DBackend_DX12& m_d2d) override;
   void Update(DOG::gfx::D2DBackend_DX12& m_d2d) override;
   ~UIButton();
   private:
   D2D_POINT_2F m_pos;
   D2D1_RECT_F m_textRect;
   D2D_VECTOR_2F m_size;
   std::wstring m_text;
   std::function<void(void)> m_callback;
};


class UISplashScreen : public UIElement
{
   public:
   UISplashScreen(DOG::gfx::D2DBackend_DX12& m_d2d, float width, float height, UINT id);
   void Draw(DOG::gfx::D2DBackend_DX12& m_d2d) override;
   void Update(DOG::gfx::D2DBackend_DX12& m_d2d) override;
   ~UISplashScreen();
   private:
   D2D1_RECT_F m_background;
   clock_t m_timer;
   ComPtr<ID2D1SolidColorBrush> m_splashBrush, m_textBrush;
   std::wstring m_text;
   float m_textOp, m_backOp;
};

class UIHealthBar : public UIElement
{
   private:
   D2D1_RECT_F m_background;

};