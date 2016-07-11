#include "OBSApi.h"

SceneItem::~SceneItem()
{
    delete source;
}

void SceneItem::SetName(CTSTR lpNewName)
{
    element->SetName(lpNewName);
}

void SceneItem::SetRender(bool render)
{
    element->SetInt(TEXT("render"), (int)((render)?1:0));
    bRender = render;
    CTSTR lpClass = element->GetString(TEXT("class"));

    if (bRender) {
        if (!lpClass) {
            AppWarning(TEXT("No class for source '%s' in scene '%s'"), element->GetName(), API->GetSceneElement()->GetName());
        } else {
            XElement *data = element->GetElement(TEXT("data"));
            source = API->CreateImageSource(lpClass, data);
            if(!source) {
                AppWarning(TEXT("Could not create image source '%s' in scene '%s'"), element->GetName(), API->GetSceneElement()->GetName());
            } else {
                API->EnterSceneMutex();
                if (parent && parent->bSceneStarted) {
                    source->BeginScene();

                    if(scmp(lpClass, L"GlobalSource") == 0)
                        source->GlobalSourceEnterScene();
                }
                API->LeaveSceneMutex();
            }
        }
    } else {
        if (source) {
            API->EnterSceneMutex();

            ImageSource *src = source;
            source = NULL;

            if(scmp(lpClass, L"GlobalSource") == 0)
                src->GlobalSourceLeaveScene();

            if (parent && parent->bSceneStarted)
                src->EndScene();
            delete src;

            API->LeaveSceneMutex();
        }
    }
}

Vect4 SceneItem::GetCrop()
{
    Vect4 scaledCrop = crop;
    Vect2 scale = GetScale();
    scaledCrop.x /= scale.x; scaledCrop.y /= scale.y;
    scaledCrop.z /= scale.y; scaledCrop.w /= scale.x;
    return scaledCrop;
}

// The following functions return the difference in x/y coordinates, not the absolute distances.
Vect2 SceneItem::GetCropTL()
{
    return Vect2(GetCrop().x, GetCrop().y);
}

Vect2 SceneItem::GetCropTR()
{
    return Vect2(-GetCrop().w, GetCrop().y);
}

Vect2 SceneItem::GetCropBR()
{
    return Vect2(-GetCrop().w, -GetCrop().z);
}

Vect2 SceneItem::GetCropBL()
{
    return Vect2(GetCrop().x, -GetCrop().z);
}


void SceneItem::Update()
{
    pos = Vect2(element->GetFloat(TEXT("x")), element->GetFloat(TEXT("y")));
    size = Vect2(element->GetFloat(TEXT("cx"), 100.0f), element->GetFloat(TEXT("cy"), 100.0f));

    //just reset the size if configuration changed, that way the user doesn't have to screw with the size
    //if(source) size = source->GetSize();
    /*element->SetInt(TEXT("cx"), int(size.x));
    element->SetInt(TEXT("cy"), int(size.y));*/
}

// void SceneItem::MoveUp()
// {
//     SceneItem *thisItem = this;
//     UINT id = parent->sceneItems.FindValueIndex(thisItem);
//     assert(id != INVALID);
// 
//     if(id > 0)
//     {
//         API->EnterSceneMutex();
// 
//         parent->sceneItems.SwapValues(id, id-1);
//         GetElement()->MoveUp();
// 
//         API->LeaveSceneMutex();
//     }
// }
// 
// void SceneItem::MoveDown()
// {
//     SceneItem *thisItem = this;
//     UINT id = parent->sceneItems.FindValueIndex(thisItem);
//     assert(id != INVALID);
// 
//     if(id < (parent->sceneItems.Num()-1))
//     {
//         API->EnterSceneMutex();
// 
//         parent->sceneItems.SwapValues(id, id+1);
//         GetElement()->MoveDown();
// 
//         API->LeaveSceneMutex();
//     }
// }
// 
// void SceneItem::MoveToTop()
// {
//     SceneItem *thisItem = this;
//     UINT id = parent->sceneItems.FindValueIndex(thisItem);
//     assert(id != INVALID);
// 
//     if(id > 0)
//     {
//         API->EnterSceneMutex();
// 
//         parent->sceneItems.Remove(id);
//         parent->sceneItems.Insert(0, this);
// 
//         GetElement()->MoveToTop();
// 
//         API->LeaveSceneMutex();
//     }
// }
// 
// void SceneItem::MoveToBottom()
// {
//     SceneItem *thisItem = this;
//     UINT id = parent->sceneItems.FindValueIndex(thisItem);
//     assert(id != INVALID);
// 
//     if(id < (parent->sceneItems.Num()-1))
//     {
//         API->EnterSceneMutex();
// 
//         parent->sceneItems.Remove(id);
//         parent->sceneItems << this;
// 
//         GetElement()->MoveToBottom();
// 
//         API->LeaveSceneMutex();
//     }
// }
// 
// //====================================================================================

Scene::~Scene()
{
    for(UINT i=0; i<sceneItems.Num(); i++)
        delete sceneItems[i];
}

SceneItem* Scene::AddImageSource(XElement *sourceElement)
{
    return InsertImageSource(sceneItems.Num(), sourceElement);
}

SceneItem* Scene::InsertImageSource(UINT pos, XElement *sourceElement)
{
    if(GetSceneItem(sourceElement->GetName()) != NULL)
    {
        AppWarning(TEXT("Scene source '%s' already in scene.  actually, no one should get this error.  if you do send it to jim immidiately."), sourceElement->GetName());
        return NULL;
    }

    if(pos > sceneItems.Num())
    {
        AppWarning(TEXT("Scene::InsertImageSource: pos >= sceneItems.Num()"));
        pos = sceneItems.Num();
    }

    bool render = sourceElement->GetInt(TEXT("render"), 1) > 0;

    float x  = sourceElement->GetFloat(TEXT("x"));
    float y  = sourceElement->GetFloat(TEXT("y"));
    float cx = sourceElement->GetFloat(TEXT("cx"), 100);
    float cy = sourceElement->GetFloat(TEXT("cy"), 100);

    SceneItem *item = new SceneItem;
    item->element = sourceElement;
    item->parent = this;
    item->pos = Vect2(x, y);
    item->size = Vect2(cx, cy);
    item->crop.w = sourceElement->GetFloat(TEXT("crop.right"));
    item->crop.x = sourceElement->GetFloat(TEXT("crop.left"));
    item->crop.y = sourceElement->GetFloat(TEXT("crop.top"));
    item->crop.z = sourceElement->GetFloat(TEXT("crop.bottom"));
    item->SetRender(render);

    API->EnterSceneMutex();
    sceneItems.Insert(pos, item);
    API->LeaveSceneMutex();

    /*if(!source)
        bMissingSources = true;*/

    return item;
}

void Scene::RemoveImageSource(SceneItem *item)
{
    if(bSceneStarted && item->source) item->source->EndScene();
    item->GetElement()->GetParent()->RemoveElement(item->GetElement());
    sceneItems.RemoveItem(item);
    delete item;
}

void Scene::RemoveImageSource(CTSTR lpName)
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        if(scmpi(sceneItems[i]->GetName(), lpName) == 0)
        {
            RemoveImageSource(sceneItems[i]);
            return;
        }
    }
}

void Scene::Preprocess()
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source && item->bRender)
            item->source->Preprocess();
    }
}

void Scene::Tick(float fSeconds)
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->Tick(fSeconds);
    }
}

void Scene::BeginScene()
{
    if(bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->BeginScene();
    }

    bSceneStarted = true;
}

void Scene::EndScene()
{
    if(!bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->EndScene();
    }

    bSceneStarted = false;
}
