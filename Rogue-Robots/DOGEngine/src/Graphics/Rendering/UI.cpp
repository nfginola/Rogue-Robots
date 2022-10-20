#include "UI.h"
#include "../../Input/Mouse.h"

DOG::UI::UI(DOG::gfx::RenderDevice* rd, DOG::gfx::Swapchain* sc, u_int numBuffers, UINT clientWidth, UINT clientHeight) : m_visible(true)
{
   srand((UINT)time(NULL));
   m_width = clientWidth;
   m_height = clientHeight;
   m_d2d = std::make_unique<DOG::gfx::D2DBackend_DX12>(rd, sc, numBuffers);
}

DOG::UI::~UI()
{

}

DOG::gfx::D2DBackend_DX12* DOG::UI::GetBackend()
{
   return m_d2d.get();
}

void DOG::UI::DrawUI()
{
   if (m_visible && m_scenes.size() > 0)
   {
      for (auto&& e : m_scenes[m_currsceneIndex]->GetScene())
      {
         e->Update(*m_d2d);
         e->Draw(*m_d2d);
      }
   }
}

void DOG::UI::Resize(UINT clientWidth, UINT clientHeight)
{

   m_width = clientWidth;
   m_height = clientHeight;
   m_d2d->OnResize();
}

void DOG::UI::FreeResize()
{

   for (auto&& e : m_scenes)
   {
      e->GetScene().clear();
   }

   m_d2d->FreeResize();
}

/// @brief Generates a unique ID
/// @return Unique ID
UINT DOG::UI::GenerateUID()
{
   UINT uid;
   do
      uid = rand();
   while (std::find(m_generatedIDs.begin(), m_generatedIDs.end(), uid) != m_generatedIDs.end());
   m_generatedIDs.push_back(uid);
   return uid;
}

/// @brief Querrys a scene from a specific sceneID
/// @param sceneID The ID of the scene
/// @return Returns the index of the scene witrh the specific sceneID. If scene is not found UINT_MAX is returned;
UINT DOG::UI::QueryScene(UINT sceneID)
{
   UINT index;
   auto res = std::find_if(m_scenes.begin(), m_scenes.end(), [&](std::unique_ptr<UIScene> const& s) { return s->GetID() == sceneID; });
   if (res == m_scenes.end())
      return UINT_MAX;
   else
   {
      ptrdiff_t ptrdiff = std::distance(m_scenes.begin(), res);
      HRESULT hr = S_OK;
      hr = PtrdiffTToUInt(ptrdiff, &index);
      HR_VFY(hr);
      return index;
   }
}

/// @brief Adds a ui element to a specific UIscene
/// @param sceneID The ID of the scene
/// @param element The element to be added
/// @return The unique ID of the element. If the scene does not exist UINT_MAX is returned
UINT DOG::UI::AddUIElementToScene(UINT sceneID, std::unique_ptr<UIElement> element)
{
   UINT index = QueryScene(sceneID);
   UINT id;
   if (index == UINT_MAX)
      return UINT_MAX;
   else
   {
      id = element->GetID();
      m_scenes[index]->GetScene().push_back(std::move(element));
   }
   return id;
}

/// @brief Adds a scene to the UI manager
/// @return A unique ID for the newly created scene
UINT DOG::UI::AddScene()
{
   auto scene = std::make_unique<UIScene>(GenerateUID());
   UINT id = scene->GetID();
   m_scenes.push_back(std::move(scene));
   return id;
}

/// @brief Removes a scene with a specific ID
/// @param sceneID The ID of the scene to be removed
void DOG::UI::RemoveScene(UINT sceneID)
{
   UINT index = QueryScene(sceneID);
   if (index == UINT_MAX)
      return;
   else
      m_scenes.erase(m_scenes.begin() + index);
}


/// @brief Changes the current scene to a scene with a specific ID
/// @param sceneID The ID of the scene to switch to
void DOG::UI::ChangeUIscene(UINT sceneID)
{
   UINT index = QueryScene(sceneID);
   if (index == UINT_MAX)
      return;
   else
   {
      m_currsceneIndex = index;
      m_currsceneID = sceneID;
   }
}

UINT DOG::UIScene::GetID()
{
   return m_ID;
}

std::vector<std::unique_ptr<DOG::UIElement>>& DOG::UIScene::GetScene()
{
   return m_scene;
}

DOG::UIElement::UIElement(UINT id) : m_ID(id)
{

}

DOG::UIElement::~UIElement()
{
}

void DOG::UIElement::Update(DOG::gfx::D2DBackend_DX12& d2d)
{
   UNREFERENCED_PARAMETER(d2d);
   return;
}

UINT DOG::UIElement::GetID()
{
   return m_ID;
}

DOG::UIButton::UIButton(DOG::gfx::D2DBackend_DX12& d2d, float x, float y, float width, float height, float fontSize, std::wstring text, std::function<void(void)> callback, UINT id) : pressed(false), m_callback(callback), UIElement(id)
{
   this->m_size = D2D1::Vector2F(width, height);
   m_textRect = D2D1::RectF(x, y, x + width, y + height);
   this->m_text = text;
   HRESULT hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &m_brush);
   HR_VFY(hr);
   hr = d2d.GetDWriteFactory()->CreateTextFormat(
      L"Robot Radicals",
      NULL,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      fontSize,
      L"en-us",
      &m_format
   );
   HR_VFY(hr);
   hr = m_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
   HR_VFY(hr);
   hr = m_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
   HR_VFY(hr);
}

DOG::UIButton::~UIButton()
{

}

void DOG::UIButton::Draw(DOG::gfx::D2DBackend_DX12& d2d)
{
   d2d.Get2DDeviceContext()->DrawRectangle(m_textRect, m_brush.Get());
   d2d.Get2DDeviceContext()->DrawTextW(
      m_text.c_str(),
      (UINT32)m_text.length(),
      m_format.Get(),
      &m_textRect,
      m_brush.Get()
   );
}

void DOG::UIButton::Update(DOG::gfx::D2DBackend_DX12& d2d)
{
   UNREFERENCED_PARAMETER(d2d);
   auto m = DOG::Mouse::GetCoordinates();
   if (m.first >= m_textRect.left && m.first <= m_textRect.right && m.second >= m_textRect.top && m.second <= m_textRect.bottom)
   {
      m_brush.Get()->SetOpacity(1.0f);
      if (DOG::Mouse::IsButtonPressed(DOG::Button::Left))
         m_callback();
   }
   else
      m_brush.Get()->SetOpacity(0.5f);

}

DOG::UISplashScreen::UISplashScreen(DOG::gfx::D2DBackend_DX12& d2d, float width, float height, UINT id) : UIElement(id)
{
   UNREFERENCED_PARAMETER(d2d);
   m_timer = clock();
   m_background = D2D1::RectF(0.0f, 0.0f, width, height);
   m_text = L"Disorganized Games";

   HRESULT hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &m_splashBrush);
   HR_VFY(hr);
   hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &m_textBrush);
   HR_VFY(hr);
   hr = d2d.GetDWriteFactory()->CreateTextFormat(
      L"Robot Radicals",
      NULL,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      50,
      L"en-us",
      &m_format
   );
   hr = m_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
   HR_VFY(hr);
   hr = m_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
   HR_VFY(hr);
   m_backOp = 1.0f;
   m_textOp = 0.0f;
}

void DOG::UISplashScreen::Draw(DOG::gfx::D2DBackend_DX12& d2d)
{
   d2d.Get2DDeviceContext()->FillRectangle(m_background, m_splashBrush.Get());
   d2d.Get2DDeviceContext()->DrawTextW(
      m_text.c_str(),
      (UINT32)m_text.length(),
      m_format.Get(),
      &m_background,
      m_textBrush.Get());
}
void DOG::UISplashScreen::Update(DOG::gfx::D2DBackend_DX12& d2d)
{
   UNREFERENCED_PARAMETER(d2d);
   float time = (float)(clock() / CLOCKS_PER_SEC);
   if (time <= 4 && time >= 0)
   {
      if (m_textOp <= 1.0f)
         m_textBrush->SetOpacity(m_textOp += 0.01f);
   }
   else
      m_textBrush->SetOpacity(m_textOp -= 0.01f);

   if (time >= 6.f)
      m_splashBrush->SetOpacity(m_backOp -= 0.01f);

}

DOG::UISplashScreen::~UISplashScreen()
{

}

float easeOutCubic(float x)
{
   return 1 - powf(1 - x, 3);
}

DOG::UIScene::UIScene(UINT id) : m_ID(id)
{

}

DOG::UIHealthBar::UIHealthBar(float x, float y, float width, float height, DOG::gfx::D2DBackend_DX12& d2d, UINT id) : UIElement(id)
{
   m_text = L"100%";
   m_value = m_test = 1.0f;
   m_barWidth = width - 2.f;
   m_border = D2D1::RectF(x, y, x + width, y + height);
   m_bar = D2D1::RectF(x + 2.0f, y + 2.0f, x + width - 2.f, y + height - 2.f);
   HRESULT hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &m_borderBrush);
   HR_VFY(hr);
   hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::GreenYellow, 0.5f), &m_barBrush);
   HR_VFY(hr);
   hr = d2d.GetDWriteFactory()->CreateTextFormat(
      L"Robot Radicals",
      NULL,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      12,
      L"en-us",
      &m_textFormat
   );
   HR_VFY(hr);
   hr = m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
   HR_VFY(hr);
   hr = m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
   HR_VFY(hr);
}

DOG::UIHealthBar::~UIHealthBar()
{

}

void DOG::UIHealthBar::Draw(DOG::gfx::D2DBackend_DX12& d2d)
{
   d2d.Get2DDeviceContext()->FillRectangle(m_bar, m_barBrush.Get());
   d2d.Get2DDeviceContext()->DrawRectangle(m_border, m_barBrush.Get());
   d2d.Get2DDeviceContext()->DrawTextW(
      m_text.c_str(),
      (UINT32)m_text.length(),
      m_textFormat.Get(),
      &m_border,
      m_borderBrush.Get());

}
void DOG::UIHealthBar::Update(DOG::gfx::D2DBackend_DX12& d2d)
{
   UNREFERENCED_PARAMETER(d2d);
   auto val = abs(sinf(m_value += 0.01f));
   m_bar.right = val * (m_barWidth)+m_bar.left - 1.0f;
   m_text = std::to_wstring((UINT)(val * 100.f + 1.f)) + L'%';
}
void DOG::UIHealthBar::SetBarValue(float value)
{
   m_value = value;
   m_text = std::to_wstring(value) + L'%';
}

DOG::UIBackground::UIBackground(float width, float heigt, std::wstring title, DOG::gfx::D2DBackend_DX12& d2d, UINT id) : UIElement(id)
{
   m_title = title;
   m_background = D2D1::RectF(0.0f, 0.0f, width, heigt);
   m_textRect = D2D1::RectF(width / 2 - 350.f / 2, heigt / 2 - 200.f, width / 2 + 300.f, heigt / 2 - 50.f);
   HRESULT hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_textBrush);
   HR_VFY(hr);
   hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_backBrush);
   HR_VFY(hr);
   hr = d2d.GetDWriteFactory()->CreateTextFormat(
      L"Robot Radicals",
      NULL,
      DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      50,
      L"en-us",
      &m_textFormat
   );
}

DOG::UIBackground::~UIBackground()
{

}

void DOG::UIBackground::Draw(DOG::gfx::D2DBackend_DX12& d2d)
{
   d2d.Get2DDeviceContext()->FillRectangle(m_background, m_backBrush.Get());
   d2d.Get2DDeviceContext()->DrawTextW(
      m_title.c_str(),
      (UINT32)m_title.length(),
      m_textFormat.Get(),
      &m_textRect,
      m_textBrush.Get());
}
void DOG::UIBackground::Update(DOG::gfx::D2DBackend_DX12& d2d)
{
   UNREFERENCED_PARAMETER(d2d);

}

DOG::UICrosshair::UICrosshair(DOG::gfx::D2DBackend_DX12& d2d, UINT id) : UIElement(id)
{
   HRESULT hr = d2d.Get2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.3f), &m_brush);
   hr = d2d.GetDWriteFactory()->CreateTextFormat(
      L"Arial",
      NULL,
      DWRITE_FONT_WEIGHT_ULTRA_LIGHT,
      DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL,
      30,
      L"en-us",
      &m_textFormat
   );
   HR_VFY(hr);
   hr = m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
   HR_VFY(hr);
   hr = m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
   HR_VFY(hr);
   auto pixsize = d2d.GetRTPixelSize();
   m_screenSize = D2D1::RectF(0.0f, 0.0f, (FLOAT)pixsize.width, (FLOAT)pixsize.height);

}

DOG::UICrosshair::~UICrosshair()
{

}

void DOG::UICrosshair::Draw(DOG::gfx::D2DBackend_DX12& d2d)
{
   d2d.Get2DDeviceContext()->DrawTextW(
      L"+",
      1u,
      m_textFormat.Get(),
      m_screenSize,
      m_brush.Get());
}