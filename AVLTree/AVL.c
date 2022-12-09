#include <stdio.h>
#include <stdlib.h>
#include "AVL.h"
#include "ListInterface.h"
#include <string.h>


//return the height of node in tree
static int AVLHeight(AVLNode node){
    if (node!=NULL)
        return node->height;
    else
        return 0;
}

static void fixHeight(AVLNode node){
    int maxHeight,left,right;
    left = AVLHeight(node->left_child);
    right = AVLHeight(node->right_child);
    if (left > right)
        maxHeight = left;
    else
        maxHeight = right;
    node->height = maxHeight + 1;
}

static int HeightDifference(AVLNode node){
    return AVLHeight(node->left_child) - AVLHeight(node->right_child);
}

static AVLNode LeftRotation(AVLNode node){
    AVLNode right = node->right_child;
    AVLNode leftTree = right->left_child;
    right->left_child = node;
    node->right_child = leftTree;
    fixHeight(node);
    fixHeight(right);
    return right;
}

static AVLNode RightRotation(AVLNode node){
    AVLNode left = node->left_child;
    AVLNode leftRight = left->right_child;
    left->right_child = node;
    node->left_child = leftRight;
    fixHeight(node);
    fixHeight(left);
    return left;
}

static AVLNode LeftRightRotation(AVLNode node){
    node->left_child = LeftRotation(node->left_child);
    return RightRotation(node);
}

static AVLNode RightLeftRotation(AVLNode node){
    node->right_child = RightRotation(node->right_child);
    return LeftRotation(node);
}

static AVLNode AVLProperty(AVLNode node){
    fixHeight(node);
    int height = HeightDifference(node);
    if (height>1){      //left subtree unbalanced
        if (HeightDifference(node->left_child)>=0)
            return RightRotation(node);
        else
            return LeftRightRotation(node);
    }
    else if (height<-1){        //right subtree unbalanced
        if (HeightDifference(node->right_child)<=0)
            return LeftRotation(node);
        else
            return RightLeftRotation(node);
    }
    return node;
}

AVLTree AVLCreate(){
    AVLTree tree = (AVLTree)malloc(sizeof(struct tree));
    if (tree==NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }
    tree->root = NULL;
    tree->size = 0;
    return tree;
}

static AVLNode newNode(time_t date){
    AVLNode newNode = (AVLNode)malloc(sizeof(struct node));
    if (newNode==NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }
    newNode->left_child = newNode->right_child = NULL ;
    newNode->date = date;
    newNode->height = 1;
    return newNode;
}

static AVLNode insert(AVLNode node,AVLItem item,time_t date,int* insertion){
    
    if (node==NULL){
        *insertion = 1;
        LList list = LCreate();
        LInsertFirst(list,item);
        AVLNode newnode = newNode(date);
        newnode->item = list;
        return newnode;
    }
    if (date==node->date){
        *insertion = 1;
        LList list = (LList)node->item;
        LInsertFirst(list,item);
    }
    else if (date < node->date){
        node->left_child = insert(node->left_child,item,date,insertion);
    }
    else{
        node->right_child = insert(node->right_child,item,date,insertion);
    }
    
    return AVLProperty(node);

}

AVLTree AVLInsert(AVLTree tree,AVLItem item,time_t date){
    int insertion=0;
    tree->root = insert(tree->root,item,date,&insertion);
    if (insertion)
        tree->size++;
    return tree;
}

static void destroy(AVLNode node){
    if (node==NULL)
        return;
    destroy(node->left_child);
    destroy(node->right_child);
    LDestroy((LList)node->item);
    free(node);
}

void AVLDestroy(AVLTree tree){
    destroy(tree->root);
    free(tree);
}

// static void countPostOrder(AVLNode node, int* counter, int min, int max, time_t date){
//     if (node==NULL)
//         return;
//     countPostOrder(node->left_child,counter,min,max,date);
//     countPostOrder(node->right_child,counter,min,max,date);
//     LList tempList = (LList)node->item;
//     *counter += LSumAge(tempList,min,max);
// }

// int AVLCount(AVLTree tree, int min, int max, time_t date){
//     int counter = 0;
//     countPostOrder(tree->root,&counter,min,max,date);
//     return counter;
// }

static void countCountryPostOrder(AVLNode node,time_t date1, time_t date2, int* counter, char *name){
    if (node==NULL)
        return;
    if (date1<node->date)
        countCountryPostOrder(node->left_child,date1,date2,counter,name);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        *counter += LSumCountry(tempList,name,date1,date2);
    }
    if (date2>node->date)
        countCountryPostOrder(node->right_child,date1,date2,counter,name);
    
}

int AVLCountCountry(AVLTree tree,time_t date1, time_t date2, char *name){
    int counter = 0;
    countCountryPostOrder(tree->root,date1,date2,&counter,name);
    return counter; 
}

static void countCountryPostOrder2(AVLNode node,time_t date1, time_t date2, int* counter, char *name){
    if (node==NULL)
        return;
    if (date1<node->date)
        countCountryPostOrder2(node->left_child,date1,date2,counter,name);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        *counter += LSumCountry2(tempList,name);
    }
    if (date2>node->date)
        countCountryPostOrder2(node->right_child,date1,date2,counter,name);
    
}

int AVLCountCountry2(AVLTree tree,time_t date1, time_t date2, char *name){
    int counter = 0;
    countCountryPostOrder2(tree->root,date1,date2,&counter,name);
    return counter; 
}



static void countrangepar(AVLNode node,time_t date1, time_t date2,int* counter,char* name){

    if (node==NULL)
        return;
    if (date1<node->date)
        countrangepar(node->left_child,date1,date2,counter,name);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        *counter += LFindSum(tempList,name);
    }
    if (date2>node->date)
        countrangepar(node->right_child,date1,date2,counter,name);
}

int AVLCountRangeKey(AVLTree tree,time_t date1, time_t date2,char* name){
    if (date1==0 && date2==0)
        return tree->size;
    else{
        int counter = 0;
        countrangepar(tree->root,date1,date2,&counter,name);
        return counter;
    }
}


static void countrange(AVLNode node, int* counter, int min, int max, time_t date){

    if (node==NULL)
        return;
    if (date<node->date)
        countrange(node->left_child,counter,min,max,date);
    if (date==node->date){
        LList tempList = (LList)node->item;
        *counter += LSumAge(tempList,min,max);
    }
    if (date>node->date)
        countrange(node->right_child,counter,min,max,date);


}


int AVLCountRange(AVLTree tree, int min, int max, time_t date){
    int counter = 0;
    countrange(tree->root,&counter,min,max,date);
    return counter;
    
}

static void countrange3(AVLNode node, int* counter, int min, int max, time_t date1, time_t date2, char *dis){

    if (node==NULL)
        return;
    if (date1<node->date)
        countrange3(node->left_child,counter,min,max,date1,date2,dis);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        *counter += LSumAge2(tempList,min,max,dis);
    }
    if (date2>node->date)
        countrange3(node->right_child,counter,min,max,date1,date2,dis);


}


int AVLCountRange3(AVLTree tree, int min, int max, time_t date1, time_t date2, char *dis){
    int counter = 0;
    countrange3(tree->root,&counter,min,max,date1,date2,dis);
    return counter;
    
}

static void countrange2(AVLNode node,time_t date1, time_t date2,int* counter){

    if (node==NULL)
        return;
    if (date1<node->date)
        countrange2(node->left_child,date1,date2,counter);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        int len = LLength(tempList);
        *counter += len;
    }
    if (date2>node->date)
        countrange2(node->right_child,date1,date2,counter);
}


int AVLCountRange2(AVLTree tree,time_t date1, time_t date2){
    if (date1==0 && date2==0)
        return tree->size;
    else{
        int counter = 0;
        countrange2(tree->root,date1,date2,&counter);
        return counter;
    }
}

static void countrangepar2(AVLNode node,time_t date1, time_t date2,int* counter,char* name){

    if (node==NULL)
        return;
    if (date1<node->date)
        countrangepar2(node->left_child,date1,date2,counter,name);
    if (date1<=node->date && date2>=node->date){
        LList tempList = (LList)node->item;
        *counter += LSumDishargeCountry(tempList,name,date1,date2);
    }
    if (date2>node->date)
        countrangepar2(node->right_child,date1,date2,counter,name);
}

int AVLCountRangeKey2(AVLTree tree,time_t date1, time_t date2,char* name){
    if (date1==0 && date2==0)
        return tree->size;
    else{
        int counter = 0;
        countrangepar2(tree->root,date1,date2,&counter,name);
        return counter;
    }
}
